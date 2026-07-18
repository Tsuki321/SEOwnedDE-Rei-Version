#include "AimbotProjectile.h"

#include "AimbotProjectilePrediction.h"
#include "../../CFG.h"
#include "../../MovementSimulation/MovementSimulation.h"
#include "../../ProjectileSim/ProjectileSim.h"

namespace
{
	constexpr float kPipeMuzzleUpZ = 200.0f;

	constexpr float kMaxSimTimeCannon     = 0.95f;
	constexpr float kMaxSimTimeIronBomber = 1.4f;
	constexpr float kMaxSimTimePipe       = 2.0f;
	constexpr float kMaxSimTimeFlame      = 0.18f;

	constexpr int kRocketSplashPointsDefault = 50;
	constexpr int kRocketSplashPointsReduced = 30;

	template <std::size_t N>
	const std::array<Vec3, N>& GetRocketSplashSpherePoints()
	{
		static const std::array<Vec3, N> points = []
		{
			std::array<Vec3, N> result = {};
			const float flGoldenAngle = static_cast<float>(PI) * (3.0f - Simd::FastSqrt(5.0f));

			for (std::size_t n = 0; n < N; n++)
			{
				const float a1 = acosf(1.0f - 2.0f * (static_cast<float>(n) / static_cast<float>(N)));
				const float a2 = flGoldenAngle * static_cast<float>(n);
				result[n] = { sinf(a1) * cosf(a2), sinf(a1) * sinf(a2), cosf(a1) };
			}

			return result;
		}();

		return points;
	}

	struct RocketSplashCandidate
	{
		Vec3 Position = {};
		float Score = 0.0f;
	};

	BallisticSolver::WeaponClass GetWeaponDragClass(C_TFWeaponBase* pWeapon)
	{
		if (!pWeapon)
			return BallisticSolver::WeaponClass::None;

		switch (pWeapon->GetWeaponID())
		{
			case TF_WEAPON_GRENADELAUNCHER:
				return (pWeapon->m_iItemDefinitionIndex() == Demoman_m_TheLochnLoad)
					? BallisticSolver::WeaponClass::LochnLoad
					: BallisticSolver::WeaponClass::GrenadeLauncher;
			case TF_WEAPON_PIPEBOMBLAUNCHER:
				return BallisticSolver::WeaponClass::Stickybomb;
			case TF_WEAPON_CANNON:
				return BallisticSolver::WeaponClass::Cannonball;
			default:
				return BallisticSolver::WeaponClass::None;
		}
	}

	float GetMuzzleUpZ(C_TFWeaponBase* pWeapon)
	{
		if (!pWeapon)
			return 0.0f;

		switch (pWeapon->GetWeaponID())
		{
			case TF_WEAPON_GRENADELAUNCHER:
			case TF_WEAPON_PIPEBOMBLAUNCHER:
			case TF_WEAPON_CANNON:
				return kPipeMuzzleUpZ;
			default:
				return 0.0f;
		}
	}

	bool IsWithinSimTimeLimit(C_TFWeaponBase* pWeapon, float flTime)
	{
		if (!pWeapon)
			return true;

		const int nWeaponID = pWeapon->GetWeaponID();

		if (nWeaponID == TF_WEAPON_CANNON)
			return flTime <= kMaxSimTimeCannon;

		if (nWeaponID == TF_WEAPON_GRENADELAUNCHER || nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER)
		{
			if (pWeapon->m_iItemDefinitionIndex() == Demoman_m_TheIronBomber)
				return flTime <= kMaxSimTimeIronBomber;
			return flTime <= kMaxSimTimePipe;
		}

		if (nWeaponID == TF_WEAPON_FLAME_BALL || nWeaponID == TF_WEAPON_FLAMETHROWER)
			return flTime <= kMaxSimTimeFlame;

		return true;
	}
}

void DrawProjPath(const CUserCmd* pCmd, float time)
{
	if (!pCmd || !G::bFiring)
		return;

	const auto pLocal = H::Entities->GetLocal();
	if (!pLocal || pLocal->deadflag())
		return;

	const auto pWeapon = H::Entities->GetWeapon();
	if (!pWeapon)
		return;

	ProjectileInfo info = {};
	if (!F::ProjectileSim->GetInfo(pLocal, pWeapon, pCmd->viewangles, info))
		return;

	if (!F::ProjectileSim->Init(info))
		return;

	for (auto n = 0; n < TIME_TO_TICKS(time); n++)
	{
		auto pre{ F::ProjectileSim->GetOrigin() };
		F::ProjectileSim->RunTick();
		auto post{ F::ProjectileSim->GetOrigin() };
		I::DebugOverlay->AddLineOverlay(pre, post, 255, 255, 255, false, 10.0f);
	}
}

void DrawMovePath(const std::vector<Vec3>& vPath)
{
	if (CFG::Visuals_Draw_Movement_Path_Style == 1)
	{
		for (size_t n = 1; n < vPath.size(); n++)
			I::DebugOverlay->AddLineOverlay(vPath[n], vPath[n - 1], 255, 255, 255, false, 10.0f);
	}

	if (CFG::Visuals_Draw_Movement_Path_Style == 2)
	{
		for (size_t n = 1; n < vPath.size(); n++)
		{
			if (n % 2 == 0)
				continue;
			I::DebugOverlay->AddLineOverlay(vPath[n], vPath[n - 1], 255, 255, 255, false, 10.0f);
		}
	}

	if (CFG::Visuals_Draw_Movement_Path_Style == 3)
	{
		for (size_t n = 1; n < vPath.size(); n++)
		{
			if (n != 1)
			{
				Vec3 right{};
				Math::AngleVectors(Math::CalcAngle(vPath[n], vPath[n - 1]), nullptr, &right, nullptr);
				const Vec3& start{ vPath[n - 1] };
				const Vec3 endL{ vPath[n - 1] + (right * 5.0f) };
				const Vec3 endR{ vPath[n - 1] - (right * 5.0f) };
				I::DebugOverlay->AddLineOverlay(start, endL, 255, 255, 255, false, 10.0f);
				I::DebugOverlay->AddLineOverlay(start, endR, 255, 255, 255, false, 10.0f);
			}
			I::DebugOverlay->AddLineOverlay(vPath[n], vPath[n - 1], 255, 255, 255, false, 10.0f);
		}
	}
}

Vec3 GetOffsetShootPos(C_TFPlayer* local, C_TFWeaponBase* weapon, const CUserCmd* pCmd)
{
	auto out{ local->GetShootPos() };

	switch (weapon->GetWeaponID())
	{
		case TF_WEAPON_ROCKETLAUNCHER:
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
		case TF_WEAPON_FLAREGUN:
		case TF_WEAPON_FLAREGUN_REVENGE:
		case TF_WEAPON_SYRINGEGUN_MEDIC:
		case TF_WEAPON_FLAME_BALL:
		case TF_WEAPON_CROSSBOW:
		case TF_WEAPON_FLAMETHROWER:
		case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
		{
			if (weapon->m_iItemDefinitionIndex() != Soldier_m_TheOriginal)
			{
				Vec3 vOffset = { 23.5f, 12.0f, -3.0f };
				if (local->m_fFlags() & FL_DUCKING)
					vOffset.z = 8.0f;
				H::AimUtils->GetProjectileFireSetup(pCmd->viewangles, vOffset, &out);
			}
			break;
		}
		case TF_WEAPON_COMPOUND_BOW:
		{
			Vec3 vOffset = { 20.5f, 12.0f, -3.0f };
			if (local->m_fFlags() & FL_DUCKING)
				vOffset.z = 8.0f;
			H::AimUtils->GetProjectileFireSetup(pCmd->viewangles, vOffset, &out);
			break;
		}
		default: break;
	}

	return out;
}

bool CAimbotProjectile::GetProjectileInfo(C_TFWeaponBase* pWeapon)
{
	m_CurProjInfo = {};

	auto curTime = [&]() -> float
	{
		if (const auto pLocal = H::Entities->GetLocal())
			return static_cast<float>(pLocal->m_nTickBase()) * I::GlobalVars->interval_per_tick;
		return I::GlobalVars->curtime;
	};

	switch (pWeapon->GetWeaponID())
	{
		case TF_WEAPON_ROCKETLAUNCHER:
		case TF_WEAPON_PARTICLE_CANNON:
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
		{
			m_CurProjInfo = { 1100.0f, 0.0f };
			m_CurProjInfo.Speed = SDKUtils::AttribHookValue(m_CurProjInfo.Speed, "mult_projectile_speed", pWeapon);

			if (C_TFPlayer* local{ H::Entities->GetLocal() })
			{
				if (const int rocket_specialist{ static_cast<int>(SDKUtils::AttribHookValue(0.0f, "rocket_specialist", local)) })
				{
					m_CurProjInfo.Speed *= Math::RemapValClamped(static_cast<float>(rocket_specialist), 1.0f, 4.0f, 1.15f, 1.6f);
					m_CurProjInfo.Speed = std::min(m_CurProjInfo.Speed, 3000.0f);
				}
			}
			break;
		}
		case TF_WEAPON_GRENADELAUNCHER:
		{
			m_CurProjInfo = { 1200.0f, 1.0f, true };
			m_CurProjInfo.Speed = SDKUtils::AttribHookValue(m_CurProjInfo.Speed, "mult_projectile_speed", pWeapon);
			break;
		}
		case TF_WEAPON_PIPEBOMBLAUNCHER:
		{
			const float flChargeBeginTime = pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime();
			const float flCharge = curTime() - flChargeBeginTime;

			if (flChargeBeginTime)
			{
				m_CurProjInfo.Speed = Math::RemapValClamped
				(
					flCharge, 0.0f,
					SDKUtils::AttribHookValue(4.0f, "stickybomb_charge_rate", pWeapon),
					900.0f, 2400.0f
				);
			}
			else
			{
				m_CurProjInfo.Speed = 900.0f;
			}

			if (m_CurProjInfo.Speed > k_flMaxVelocity)
				m_CurProjInfo.Speed = k_flMaxVelocity;

			m_CurProjInfo.GravityMod = 1.0f;
			m_CurProjInfo.Pipes = true;
			break;
		}
		case TF_WEAPON_CANNON:
		{
			m_CurProjInfo = { 1454.0f, 1.0f, true };
			break;
		}
		case TF_WEAPON_COMPOUND_BOW:
		{
			const float flChargeBeginTime = pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime();
			const float flCharge = curTime() - flChargeBeginTime;

			if (flChargeBeginTime)
			{
				m_CurProjInfo.Speed = 1800.0f + std::clamp<float>(flCharge, 0.0f, 1.0f) * 800.0f;
				m_CurProjInfo.GravityMod = Math::RemapValClamped(flCharge, 0.0f, 1.0f, 0.5f, 0.1f);
			}
			else
			{
				m_CurProjInfo.Speed = 1800.0f;
				m_CurProjInfo.GravityMod = 0.5f;
			}
			break;
		}
		case TF_WEAPON_CROSSBOW:
		case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
		{
			m_CurProjInfo = { 2400.0f, 0.2f };
			break;
		}
		case TF_WEAPON_SYRINGEGUN_MEDIC:
		{
			m_CurProjInfo = { 1000.0f, 0.3f };
			break;
		}
		case TF_WEAPON_FLAREGUN:
		{
			m_CurProjInfo = { 2000.0f, 0.3f };
			break;
		}
		case TF_WEAPON_FLAREGUN_REVENGE:
		{
			m_CurProjInfo = { 3000.0f, 0.45f };
			break;
		}
		case TF_WEAPON_FLAME_BALL:
		{
			m_CurProjInfo = { 3000.0f, 0.0f };
			break;
		}
		case TF_WEAPON_FLAMETHROWER:
		{
			m_CurProjInfo = { 2000.0f, 0.0f };
			m_CurProjInfo.Flamethrower = true;
			break;
		}
		case TF_WEAPON_RAYGUN:
		case TF_WEAPON_DRG_POMSON:
		{
			m_CurProjInfo = { 1200.0f, 0.0f };
			break;
		}
		default: break;
	}

	return m_CurProjInfo.Speed > 0.0f;
}

bool CAimbotProjectile::CalcProjAngle(const Vec3& vFrom, const Vec3& vTo, Vec3& vAngleOut, float& flTimeOut)
{
	const auto pWeapon = H::Entities->GetWeapon();
	if (!pWeapon)
		return false;

	BallisticSolver::SolverParams params;
	params.ShootPos   = vFrom;
	params.TargetPos  = vTo;
	params.TargetVel  = Vec3(0.0f, 0.0f, 0.0f);
	params.Speed      = m_CurProjInfo.Speed;
	params.Gravity    = SDKUtils::GetGravity() * m_CurProjInfo.GravityMod;
	params.MuzzleUpZ  = GetMuzzleUpZ(pWeapon);
	params.UseHighArc = CFG::Aimbot_Projectile_High_Arc;
	params.DragCoeff   = BallisticSolver::ComputeDragCoefficient(GetWeaponDragClass(pWeapon));
	params.DragIters   = 3;

	BallisticSolver::SolveResult result = BallisticSolver::SolveBallistic(params);
	if (!result.Valid)
		return false;

	if (!IsWithinSimTimeLimit(pWeapon, result.Time))
		return false;

	vAngleOut = BallisticSolver::DirectionToAngles(result.Direction);
	flTimeOut = result.Time;

	return true;
}

void CAimbotProjectile::OffsetPlayerPosition(C_TFWeaponBase* pWeapon, Vec3& vPos, C_TFPlayer* pPlayer, bool bDucked, bool bOnGround, int aimPosition)
{
	const float flMaxZ{ (bDucked ? 62.0f : 82.0f) * pPlayer->m_flModelScale() };

	switch (aimPosition)
	{
		case 0:
		{
			vPos.z += (flMaxZ * 0.2f);
			m_LastAimPos = 0;
			break;
		}
		case 1:
		{
			vPos.z += (flMaxZ * 0.5f);
			m_LastAimPos = 1;
			break;
		}
		case 2:
		{
			if (CFG::Aimbot_Projectile_Advanced_Head_Aim)
			{
				const Vec3 vDelta = pPlayer->GetHitboxPos(HITBOX_HEAD) - pPlayer->m_vecOrigin();
				vPos.x += vDelta.x;
				vPos.y += vDelta.y;
			}
			vPos.z += (flMaxZ * 0.85f);
			m_LastAimPos = 2;
			break;
		}
		case 3:
		{
			if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
			{
				if (CFG::Aimbot_Projectile_Advanced_Head_Aim)
				{
					const Vec3 vDelta = pPlayer->GetHitboxPos(HITBOX_HEAD) - pPlayer->m_vecOrigin();
					vPos.x += vDelta.x;
					vPos.y += vDelta.y;
				}
				vPos.z += (flMaxZ * 0.92f);
				m_LastAimPos = 2;
			}
			else
			{
				switch (pWeapon->GetWeaponID())
				{
					case TF_WEAPON_ROCKETLAUNCHER:
					case TF_WEAPON_PARTICLE_CANNON:
					case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
					case TF_WEAPON_GRENADELAUNCHER:
					case TF_WEAPON_CANNON:
					{
						if (bOnGround)
						{
							vPos.z += (flMaxZ * 0.2f);
							m_LastAimPos = 0;
						}
						else
						{
							vPos.z += (flMaxZ * 0.5f);
							m_LastAimPos = 1;
						}
						break;
					}
					case TF_WEAPON_PIPEBOMBLAUNCHER:
					{
						vPos.z += (flMaxZ * 0.1f);
						m_LastAimPos = 0;
						break;
					}
					default:
					{
						vPos.z += (flMaxZ * 0.5f);
						m_LastAimPos = 1;
						break;
					}
				}
			}
			break;
		}
		default: break;
	}
}

bool CAimbotProjectile::CanArcReach(const Vec3& vFrom, const Vec3& vTo, const Vec3& vAngleTo, float flTargetTime, C_BaseEntity* pTarget)
{
	const auto pLocal = H::Entities->GetLocal();
	if (!pLocal)
		return false;

	const auto pWeapon = H::Entities->GetWeapon();
	if (!pWeapon)
		return false;

	ProjectileInfo info{};
	if (!F::ProjectileSim->GetInfo(pLocal, pWeapon, vAngleTo, info))
		return false;

	if (!F::ProjectileSim->Init(info, true))
		return false;

	CTraceFilterWorldCustom filter{};
	filter.m_pTarget = pTarget;

	for (auto n = 0; n < TIME_TO_TICKS(flTargetTime * 1.2f); n++)
	{
		auto pre{ F::ProjectileSim->GetOrigin() };
		F::ProjectileSim->RunTick();
		auto post{ F::ProjectileSim->GetOrigin() };

		trace_t trace{};

		Vec3 mins{ -6.0f, -6.0f, -6.0f };
		Vec3 maxs{ 6.0f, 6.0f, 6.0f };

		switch (info.m_type)
		{
			case TF_PROJECTILE_PIPEBOMB:
			case TF_PROJECTILE_PIPEBOMB_REMOTE:
			case TF_PROJECTILE_PIPEBOMB_PRACTICE:
			case TF_PROJECTILE_CANNONBALL:
			{
				mins = { -8.0f, -8.0f, -8.0f };
				maxs = { 8.0f, 8.0f, 20.0f };
				break;
			}
			case TF_PROJECTILE_FLARE:
			{
				mins = { -8.0f, -8.0f, -8.0f };
				maxs = { 8.0f, 8.0f, 8.0f };
				break;
			}
			default: break;
		}

		H::AimUtils->TraceHull(pre, post, mins, maxs, MASK_SOLID, &filter, &trace);

		if (trace.m_pEnt == pTarget)
			return true;

		if (trace.DidHit())
		{
			if (info.m_pos.DistTo(trace.endpos) > info.m_pos.DistTo(vTo))
				return true;

			if (trace.endpos.DistTo(vTo) > 40.0f)
				return false;

			H::AimUtils->Trace(trace.endpos, vTo, MASK_SOLID, &filter, &trace);
			return !trace.DidHit() || trace.m_pEnt == pTarget;
		}
	}

	if (F::ProjectileSim->GetOrigin().DistTo(vTo) > 80.0f)
		return false;

	return true;
}

bool CAimbotProjectile::CanSee(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vFrom, const Vec3& vTo, const ProjTarget_t& target, float flTargetTime)
{
	Vec3 vLocalPos = vFrom;

	switch (pWeapon->GetWeaponID())
	{
		case TF_WEAPON_ROCKETLAUNCHER:
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
		case TF_WEAPON_FLAREGUN:
		case TF_WEAPON_FLAREGUN_REVENGE:
		case TF_WEAPON_SYRINGEGUN_MEDIC:
		case TF_WEAPON_FLAME_BALL:
		case TF_WEAPON_CROSSBOW:
		case TF_WEAPON_FLAMETHROWER:
		case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
		{
			if (pWeapon->m_iItemDefinitionIndex() != Soldier_m_TheOriginal)
			{
				Vec3 vOffset = { 23.5f, 12.0f, -3.0f };
				if (pLocal->m_fFlags() & FL_DUCKING)
					vOffset.z = 8.0f;
				H::AimUtils->GetProjectileFireSetup(target.AngleTo, vOffset, &vLocalPos);
			}
			break;
		}
		case TF_WEAPON_COMPOUND_BOW:
		{
			Vec3 vOffset = { 20.5f, 12.0f, -3.0f };
			if (pLocal->m_fFlags() & FL_DUCKING)
				vOffset.z = 8.0f;
			H::AimUtils->GetProjectileFireSetup(target.AngleTo, vOffset, &vLocalPos);
			break;
		}
		default: break;
	}

	if (m_CurProjInfo.GravityMod != 0.f)
		return CanArcReach(vFrom, vTo, target.AngleTo, flTargetTime, target.Entity);

	if (m_CurProjInfo.Flamethrower)
		return H::AimUtils->TraceFlames(target.Entity, vLocalPos, vTo);

	return H::AimUtils->TraceProjectile(target.Entity, vLocalPos, vTo);
}

bool CAimbotProjectile::RunSplash(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd,
                                  const Vec3& vLocalPos, const Vec3& center, ProjTarget_t& target)
{
	const auto isRocketLauncher{ pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER };
	const auto isDirectHit{ pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT };
	const auto isAirStrike{ pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheAirStrike };

	if (!isRocketLauncher && !isDirectHit && !isAirStrike)
		return false;

	const Vec3 vShooterDir = (vLocalPos - center).Normalized();

	const int numPoints = CFG::Aimbot_Projectile_Rocket_Splash == 2 ? kRocketSplashPointsReduced : kRocketSplashPointsDefault;
	const Vec3* pSpherePoints = CFG::Aimbot_Projectile_Rocket_Splash == 2
		? GetRocketSplashSpherePoints<kRocketSplashPointsReduced>().data()
		: GetRocketSplashSpherePoints<kRocketSplashPointsDefault>().data();
	auto radius{ isRocketLauncher ? 180.0f : 80.0f };
	if (isAirStrike)
		radius = 130.0f;

	std::array<RocketSplashCandidate, kRocketSplashPointsDefault> potential = {};
	std::size_t potentialCount = 0;

	CTraceFilterWorldCustom filterGen{};
	trace_t traceGen{};

	for (int n = 0; n < numPoints; n++)
	{
		Vec3 spherePoint = pSpherePoints[n];
		const float flFacing = spherePoint.Dot(vShooterDir);
		if (flFacing < 0.0f)
			spherePoint = spherePoint - vShooterDir.Scale(2.0f * flFacing);

		auto point{ center + spherePoint.Scale(radius) };

		H::AimUtils->Trace(center, point, MASK_SOLID, &filterGen, &traceGen);

		if (traceGen.fraction > 0.99f)
			continue;

		const Vec3 vPosition = traceGen.endpos;
		potential[potentialCount++] = {
			vPosition,
			vPosition.DistTo(center) + vPosition.DistTo(vLocalPos) * 0.3f
		};
	}

	if (potentialCount == 0)
		return false;

	std::sort(potential.begin(), potential.begin() + potentialCount, [](const RocketSplashCandidate& a, const RocketSplashCandidate& b)
	{
		return a.Score < b.Score;
	});

	trace_t traceVal{};
	CTraceFilterWorldCustom filterVal{};
	const Vec3 vOffsetShootPos = GetOffsetShootPos(pLocal, pWeapon, pCmd);

	for (std::size_t n = 0; n < potentialCount; n++)
	{
		const Vec3& point = potential[n].Position;

		if (!CalcProjAngle(vLocalPos, point, target.AngleTo, target.TimeToTarget))
			continue;

		H::AimUtils->TraceHull
		(
			vOffsetShootPos,
			point,
			{ -4.0f, -4.0f, -4.0f },
			{ 4.0f, 4.0f, 4.0f },
			MASK_SOLID,
			&filterVal,
			&traceVal
		);

		if (traceVal.fraction < 0.9f || traceVal.startsolid || traceVal.allsolid)
			continue;

		H::AimUtils->Trace(traceVal.endpos, point, MASK_SOLID, &filterVal, &traceVal);

		if (traceVal.fraction < 1.0f)
			continue;

		return true;
	}

	return false;
}

bool CAimbotProjectile::SolveTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd, ProjTarget_t& target)
{
	Vec3 vLocalPos = pLocal->GetShootPos();

	if (m_CurProjInfo.Pipes)
	{
		const Vec3 vOffset = { 16.0f, 8.0f, -6.0f };
		H::AimUtils->GetProjectileFireSetup(pCmd->viewangles, vOffset, &vLocalPos);
	}

	m_TargetPath.clear();

	const float gravity   = SDKUtils::GetGravity() * m_CurProjInfo.GravityMod;
	const float muzzleUpZ = GetMuzzleUpZ(pWeapon);
	const float dragCoeff = BallisticSolver::ComputeDragCoefficient(GetWeaponDragClass(pWeapon));
	const bool  useHighArc = CFG::Aimbot_Projectile_High_Arc;

	if (target.Entity->GetClassId() == ETFClassIds::CTFPlayer)
	{
		const auto pPlayer = target.Entity->As<C_TFPlayer>();

		if (CFG::Aimbot_Projectile_Hitchance_Enabled)
		{
			const int iSamples = pPlayer->m_fFlags() & FL_ONGROUND ? 4 : 3;
			const float flHitchance = F::MovementSimulation->CalculateHitchance(pPlayer, iSamples);
			const float flMinimum = CFG::Aimbot_Projectile_Hitchance_Minimum / 100.0f;

			if (flHitchance < flMinimum)
				return false;
		}

		CMovementSimScope simScope(pPlayer);
		if (!simScope)
			return false;

		const Vec3 targetVel = pPlayer->m_vecVelocity();

		BallisticSolver::SolverParams initParams;
		initParams.ShootPos   = vLocalPos;
		initParams.TargetPos  = F::MovementSimulation->GetOrigin();
		initParams.TargetVel  = targetVel;
		initParams.Speed      = m_CurProjInfo.Speed;
		initParams.Gravity    = gravity;
		initParams.MuzzleUpZ  = muzzleUpZ;
		initParams.UseHighArc = useHighArc;
		initParams.DragCoeff   = dragCoeff;
		initParams.DragIters   = 3;

		BallisticSolver::SolveResult initResult = BallisticSolver::SolveBallistic(initParams);
		const float flMaxSimulationTime = std::max(TICK_INTERVAL, CFG::Aimbot_Projectile_Max_Simulation_Time);
		const int hardMaxTicks = std::max(1, TIME_TO_TICKS(flMaxSimulationTime));
		const float flTimingBias = ProjectilePredictionMath::ComputeTimingBias(SDKUtils::GetLatency(), SDKUtils::GetLerp());

		int simulationTicks = hardMaxTicks;
		if (initResult.Valid && initResult.Time > 0.0f)
		{
			const float flMargin = std::max(0.2f, initResult.Time * 0.2f);
			const float flEstimatedHorizon = std::min(flMaxSimulationTime, initResult.Time + flTimingBias + flMargin);
			simulationTicks = std::clamp(TIME_TO_TICKS(flEstimatedHorizon) + 1, 1, hardMaxTicks);
		}

		m_TargetPath.reserve(simulationTicks);
		auto simulateToTickCount = [&](int tickCount)
		{
			while (static_cast<int>(m_TargetPath.size()) < tickCount)
			{
				const int nTick = static_cast<int>(m_TargetPath.size());
				m_TargetPath.push_back(F::MovementSimulation->GetOrigin());
				F::MovementSimulation->RunTick(TICKS_TO_TIME(nTick));
			}
		};

		simulateToTickCount(simulationTicks);
		if (m_TargetPath.empty())
			return false;

		auto refinePath = [&]()
		{
			int startTick = -1;
			if (initResult.Valid && initResult.Time > 0.0f)
			{
				startTick = std::clamp(TIME_TO_TICKS(initResult.Time), 0, static_cast<int>(m_TargetPath.size()) - 1);
			}
			else
			{
				startTick = BallisticSolver::BinarySearchMeetingTick(
					m_TargetPath, TICK_INTERVAL, vLocalPos, m_CurProjInfo.Speed,
					gravity, muzzleUpZ, dragCoeff, flMaxSimulationTime, useHighArc);
			}

			return BallisticSolver::NewtonRefineOverPath(
				m_TargetPath, TICK_INTERVAL, startTick, vLocalPos,
				m_CurProjInfo.Speed, gravity, muzzleUpZ, dragCoeff, useHighArc, 3);
		};

		BallisticSolver::NewtonRefineResult refineResult = refinePath();

		if (!refineResult.Valid)
			return false;

		for (int nExtension = 0; nExtension < 2; nExtension++)
		{
			const float flRequiredHorizon = refineResult.Time + flTimingBias + TICKS_TO_TIME(4);
			const int requiredTicks = std::clamp(TIME_TO_TICKS(flRequiredHorizon) + 1, 1, hardMaxTicks);
			if (requiredTicks <= static_cast<int>(m_TargetPath.size()))
				break;

			simulateToTickCount(requiredTicks);
			refineResult = refinePath();
			if (!refineResult.Valid)
				return false;
		}

		const int meetingTick = std::clamp(refineResult.Tick, 0, static_cast<int>(m_TargetPath.size()) - 1);

		const bool bDucked = pPlayer->m_fFlags() & FL_DUCKING;
		const bool bOnGround = pPlayer->m_fFlags() & FL_ONGROUND;

		Vec3 vTarget = m_TargetPath[meetingTick];
		OffsetPlayerPosition(pWeapon, vTarget, pPlayer, bDucked, bOnGround, CFG::Aimbot_Projectile_Aim_Position);

		BallisticSolver::SolverParams aimParams;
		aimParams.ShootPos   = vLocalPos;
		aimParams.TargetPos  = vTarget;
		aimParams.TargetVel  = Vec3(0.0f, 0.0f, 0.0f);
		aimParams.Speed      = m_CurProjInfo.Speed;
		aimParams.Gravity    = gravity;
		aimParams.MuzzleUpZ  = muzzleUpZ;
		aimParams.UseHighArc = useHighArc;
		aimParams.DragCoeff   = dragCoeff;
		aimParams.DragIters   = 3;

		BallisticSolver::SolveResult aimResult = BallisticSolver::SolveBallistic(aimParams);
		if (!aimResult.Valid)
			return false;

		if (!IsWithinSimTimeLimit(pWeapon, aimResult.Time))
			return false;

		target.AngleTo = BallisticSolver::DirectionToAngles(aimResult.Direction);
		target.TimeToTarget = aimResult.Time;

		if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
		{
			const auto sticky_arm_time{ SDKUtils::AttribHookValue(0.8f, "sticky_arm_time", pLocal) };
			if (aimResult.Time < sticky_arm_time)
				return false;
		}

		if (CFG::Aimbot_Projectile_Rocket_Splash == 2)
		{
			const Vec3 splashCenter = m_TargetPath[meetingTick] + Vec3(0.0f, 0.0f, 0.0f);
			if (RunSplash(pLocal, pWeapon, pCmd, vLocalPos, splashCenter, target))
				return true;
		}

		if (CanSee(pLocal, pWeapon, vLocalPos, vTarget, target, aimResult.Time))
			return true;

		if (CFG::Aimbot_Projectile_BBOX_Multipoint && pWeapon->GetWeaponID() != TF_WEAPON_COMPOUND_BOW)
		{
			for (int n = 0; n < 3; n++)
			{
				if (n == m_LastAimPos)
					continue;

				Vec3 vTargetMp = m_TargetPath[meetingTick];
				OffsetPlayerPosition(pWeapon, vTargetMp, pPlayer, bDucked, bOnGround, n);

				BallisticSolver::SolverParams mpParams;
				mpParams.ShootPos   = vLocalPos;
				mpParams.TargetPos  = vTargetMp;
				mpParams.Speed      = m_CurProjInfo.Speed;
				mpParams.Gravity    = gravity;
				mpParams.MuzzleUpZ  = muzzleUpZ;
				mpParams.UseHighArc = useHighArc;
				mpParams.DragCoeff   = dragCoeff;
				mpParams.DragIters   = 3;

				BallisticSolver::SolveResult mpResult = BallisticSolver::SolveBallistic(mpParams);
				if (!mpResult.Valid)
					continue;

				target.AngleTo = BallisticSolver::DirectionToAngles(mpResult.Direction);
				target.TimeToTarget = mpResult.Time;

				if (CanSee(pLocal, pWeapon, vLocalPos, vTargetMp, target, mpResult.Time))
					return true;
			}
		}

		if (CFG::Aimbot_Projectile_Rocket_Splash == 1)
		{
			const Vec3 splashCenter = m_TargetPath[meetingTick];
			if (RunSplash(pLocal, pWeapon, pCmd, vLocalPos, splashCenter, target))
				return true;
		}
	}
	else
	{
		Vec3 vTarget = target.Entity->GetCenter();

		BallisticSolver::SolverParams params;
		params.ShootPos   = vLocalPos;
		params.TargetPos  = vTarget;
		params.TargetVel  = Vec3(0.0f, 0.0f, 0.0f);
		params.Speed      = m_CurProjInfo.Speed;
		params.Gravity    = gravity;
		params.MuzzleUpZ  = muzzleUpZ;
		params.UseHighArc = useHighArc;
		params.DragCoeff   = dragCoeff;
		params.DragIters   = 3;

		BallisticSolver::SolveResult result = BallisticSolver::SolveBallistic(params);
		if (!result.Valid)
			return false;

		if (!IsWithinSimTimeLimit(pWeapon, result.Time))
			return false;

		target.AngleTo = BallisticSolver::DirectionToAngles(result.Direction);
		target.TimeToTarget = result.Time;

		const float flTimingBias = ProjectilePredictionMath::ComputeTimingBias(SDKUtils::GetLatency(), SDKUtils::GetLerp());

		if (result.Time + flTimingBias > CFG::Aimbot_Projectile_Max_Simulation_Time + TICK_INTERVAL * 2.0f)
			return false;

		if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
		{
			if (result.Time < SDKUtils::AttribHookValue(0.8f, "sticky_arm_time", pLocal))
				return false;
		}

		if (CanSee(pLocal, pWeapon, vLocalPos, vTarget, target, result.Time))
			return true;

		if (CFG::Aimbot_Projectile_Rocket_Splash && RunSplash(pLocal, pWeapon, pCmd, vLocalPos, vTarget, target))
			return true;
	}

	m_TargetPath.clear();
	return false;
}

bool CAimbotProjectile::GetTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd, ProjTarget_t& outTarget)
{
	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	const int nLocalTeam = pLocal->m_iTeamNum();
	const int nWeaponID = pWeapon->GetWeaponID();
	const int nSortMode = CFG::Aimbot_Projectile_Sort;
	const float flFOVLimit = CFG::Aimbot_Projectile_FOV;

	m_vecTargets.clear();

	if (CFG::Aimbot_Target_Players)
	{
		const auto nGroup = nWeaponID == TF_WEAPON_CROSSBOW ? EEntGroup::PLAYERS_ALL : EEntGroup::PLAYERS_ENEMIES;

		for (const auto pEntity : H::Entities->GetGroup(nGroup))
		{
			if (!pEntity || pEntity == pLocal)
				continue;

			const auto pPlayer = pEntity->As<C_TFPlayer>();

			if (pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
				continue;

			if (pPlayer->m_iTeamNum() != nLocalTeam)
			{
				if (CFG::Aimbot_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
					continue;
				if (CFG::Aimbot_Ignore_Invisible && pPlayer->IsInvisible())
					continue;
				if (CFG::Aimbot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
					continue;
				if (CFG::Aimbot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
					continue;
			}
			else
			{
				if (nWeaponID == TF_WEAPON_CROSSBOW)
				{
					if (pPlayer->m_iHealth() >= pPlayer->GetMaxHealth() || pPlayer->IsInvulnerable())
						continue;
				}
			}

			Vec3 vPos = pPlayer->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = nSortMode == 0 ? Math::CalcFov(vLocalAngles, vAngleTo) : 0.0f;
			const float flDistTo = vLocalPos.DistTo(vPos);

			if (nSortMode == 0 && flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(AimTarget_t { pPlayer, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	if (CFG::Aimbot_Target_Buildings)
	{
		const auto isRescueRanger{ nWeaponID == TF_WEAPON_SHOTGUN_BUILDING_RESCUE };

		for (const auto pEntity : H::Entities->GetGroup(isRescueRanger ? EEntGroup::BUILDINGS_ALL : EEntGroup::BUILDINGS_ENEMIES))
		{
			if (!pEntity)
				continue;

			const auto pBuilding = pEntity->As<C_BaseObject>();

			if (pBuilding->m_bPlacing())
				continue;

			if (isRescueRanger && pBuilding->m_iTeamNum() == nLocalTeam && pBuilding->m_iHealth() >= pBuilding->m_iMaxHealth())
				continue;

			Vec3 vPos = pBuilding->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = nSortMode == 0 ? Math::CalcFov(vLocalAngles, vAngleTo) : 0.0f;
			const float flDistTo = vLocalPos.DistTo(vPos);

			if (nSortMode == 0 && flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(AimTarget_t { pBuilding, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	if (m_vecTargets.empty())
		return false;

	F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Projectile_Sort);

	const auto maxTargets{ std::min(CFG::Aimbot_Projectile_Max_Processing_Targets, static_cast<int>(m_vecTargets.size())) };
	auto targetsScanned{ 0 };

	for (auto& target : m_vecTargets)
	{
		if (targetsScanned >= maxTargets)
			break;

		targetsScanned++;

		if (!SolveTarget(pLocal, pWeapon, pCmd, target))
			continue;

		if (CFG::Aimbot_Projectile_Sort == 0 && Math::CalcFov(vLocalAngles, target.AngleTo) > CFG::Aimbot_Projectile_FOV)
			continue;

		outTarget = target;
		return true;
	}

	return false;
}

bool CAimbotProjectile::ShouldAim(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	return CFG::Aimbot_Projectile_Aim_Type != 1 || IsFiring(pCmd, pLocal, pWeapon) && pWeapon->HasPrimaryAmmoForShot();
}

void CAimbotProjectile::Aim(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vAngles)
{
	Vec3 vAngleTo = vAngles - pLocal->m_vecPunchAngle();

	Math::ClampAngles(vAngleTo);

	switch (CFG::Aimbot_Projectile_Aim_Type)
	{
		case 0:
		{
			pCmd->viewangles = vAngleTo;
			break;
		}
		case 1:
		{
			if (m_CurProjInfo.Flamethrower ? true : G::bCanPrimaryAttack)
			{
				H::AimUtils->FixMovement(pCmd, vAngleTo);
				pCmd->viewangles = vAngleTo;

				if (m_CurProjInfo.Flamethrower)
					G::bSilentAngles = true;
				else
					G::bPSilentAngles = true;
			}
			break;
		}
		default: break;
	}
}

bool CAimbotProjectile::ShouldFire(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (!CFG::Aimbot_AutoShoot)
	{
		if (pWeapon->GetWeaponID() == TF_WEAPON_FLAME_BALL && pLocal->m_flTankPressure() < 100.0f)
			pCmd->buttons &= ~IN_ATTACK;
		return false;
	}
	return true;
}

void CAimbotProjectile::HandleFire(CUserCmd* pCmd, C_TFWeaponBase* pWeapon, C_TFPlayer* pLocal, const ProjTarget_t& target)
{
	const bool bIsBazooka = pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka;
	if (!bIsBazooka && !pWeapon->HasPrimaryAmmoForShot())
		return;

	const int nWeaponID = pWeapon->GetWeaponID();
	if (nWeaponID == TF_WEAPON_COMPOUND_BOW || nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER)
	{
		if (pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime() > 0.0f)
			pCmd->buttons &= ~IN_ATTACK;
		else
			pCmd->buttons |= IN_ATTACK;
	}
	else if (nWeaponID == TF_WEAPON_CANNON)
	{
		if (CFG::Aimbot_Projectile_Auto_Double_Donk)
		{
			const float flDetonateTime = pWeapon->As<C_TFGrenadeLauncher>()->m_flDetonateTime();
			const float flDetonateMaxTime = SDKUtils::AttribHookValue(0.0f, "grenade_launcher_mortar_mode", pWeapon);
			float flCharge = Math::RemapValClamped(flDetonateTime - I::GlobalVars->curtime, 0.0f, flDetonateMaxTime, 0.0f, 1.0f);

			if (I::GlobalVars->curtime > flDetonateTime)
				flCharge = 1.0f;

			if (flCharge < target.TimeToTarget * 0.8f)
				pCmd->buttons &= ~IN_ATTACK;
			else
				pCmd->buttons |= IN_ATTACK;
		}
		else
		{
			if (pWeapon->As<C_TFGrenadeLauncher>()->m_flDetonateTime() > 0.0f)
				pCmd->buttons &= ~IN_ATTACK;
			else
				pCmd->buttons |= IN_ATTACK;
		}
	}
	else if (nWeaponID == TF_WEAPON_FLAME_BALL)
	{
		if (pLocal->m_flTankPressure() >= 100.0f)
			pCmd->buttons |= IN_ATTACK;
		else
			pCmd->buttons &= ~IN_ATTACK;
	}
	else
	{
		pCmd->buttons |= IN_ATTACK;
	}

	if (bIsBazooka && pWeapon->HasPrimaryAmmoForShot())
		pCmd->buttons &= ~IN_ATTACK;
}

bool CAimbotProjectile::IsFiring(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (!pWeapon->HasPrimaryAmmoForShot())
		return false;

	const int nWeaponID = pWeapon->GetWeaponID();
	if (nWeaponID == TF_WEAPON_COMPOUND_BOW || nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER || nWeaponID == TF_WEAPON_CANNON)
	{
		return (G::nOldButtons & IN_ATTACK) && !(pCmd->buttons & IN_ATTACK);
	}

	if (nWeaponID == TF_WEAPON_FLAME_BALL)
	{
		return pLocal->m_flTankPressure() >= 100.0f && (pCmd->buttons & IN_ATTACK);
	}

	if (pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka)
		return G::bCanPrimaryAttack;

	if (nWeaponID == TF_WEAPON_FLAMETHROWER)
	{
		return pCmd->buttons & IN_ATTACK;
	}

	return (pCmd->buttons & IN_ATTACK) && G::bCanPrimaryAttack;
}

void CAimbotProjectile::Run(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (!CFG::Aimbot_Projectile_Active)
		return;

	if (!GetProjectileInfo(pWeapon))
		return;

	if (CFG::Aimbot_Projectile_Sort == 0)
		G::flAimbotFOV = CFG::Aimbot_Projectile_FOV;

	if (Shifting::bShifting && !Shifting::bShiftingWarp)
		return;

	if (!H::Input->IsDown(CFG::Aimbot_Key))
		return;

	ProjTarget_t target = {};
	if (GetTarget(pLocal, pWeapon, pCmd, target) && target.Entity)
	{
		G::nTargetIndexEarly = target.Entity->entindex();
		G::nTargetIndex = target.Entity->entindex();

		if (ShouldFire(pCmd, pLocal, pWeapon))
			HandleFire(pCmd, pWeapon, pLocal, target);

		const bool bIsFiring = IsFiring(pCmd, pLocal, pWeapon);

		G::bFiring = bIsFiring;

		if (ShouldAim(pCmd, pLocal, pWeapon) || bIsFiring)
		{
			Aim(pCmd, pLocal, pWeapon, target.AngleTo);

			if (bIsFiring && m_TargetPath.size() > 1)
			{
				I::DebugOverlay->ClearAllOverlays();
				DrawMovePath(m_TargetPath);
				m_TargetPath.clear();
			}
		}
	}
}
