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

	float GetPredictionTimingBias(C_TFWeaponBase* pWeapon)
	{
		const bool bUsesPseudoSilent = CFG::Aimbot_Projectile_Aim_Type == 1
			&& pWeapon && pWeapon->GetWeaponID() != TF_WEAPON_FLAMETHROWER;
		const float flPacketDelay = bUsesPseudoSilent ? TICK_INTERVAL : 0.0f;
		return ProjectilePredictionMath::ComputeTimingBias(SDKUtils::GetOutgoingLatency(), SDKUtils::GetLerp()) + flPacketDelay;
	}

	float GetProjectileGravity(float flGravityMod)
	{
		const float flGravity = SDKUtils::GetGravity();
		return (flGravity > 0.0f ? flGravity : 800.0f) * flGravityMod;
	}

	void GetProjectileHull(const ProjectileInfo& info, Vec3& mins, Vec3& maxs)
	{
		mins = { -6.0f, -6.0f, -6.0f };
		maxs = { 6.0f, 6.0f, 6.0f };

		switch (info.m_type)
		{
			case TF_PROJECTILE_PIPEBOMB:
			case TF_PROJECTILE_PIPEBOMB_REMOTE:
			case TF_PROJECTILE_PIPEBOMB_PRACTICE:
			case TF_PROJECTILE_CANNONBALL:
				mins = { -8.0f, -8.0f, -8.0f };
				maxs = { 8.0f, 8.0f, 20.0f };
				break;
			case TF_PROJECTILE_FLARE:
				mins = { -8.0f, -8.0f, -8.0f };
				maxs = { 8.0f, 8.0f, 8.0f };
				break;
			case TF_PROJECTILE_FLAME_ROCKET:
				mins = { -12.0f, -12.0f, -12.0f };
				maxs = { 12.0f, 12.0f, 12.0f };
				break;
			default: break;
		}
	}

	bool IsInsideExpandedBounds(const Vec3& point, const Vec3& targetOrigin, const Vec3& targetMins,
		const Vec3& targetMaxs, const Vec3& projectileMins, const Vec3& projectileMaxs)
	{
		constexpr float kEndpointTolerance = 1.0f;
		const Vec3 mins = targetOrigin + targetMins - projectileMaxs - Vec3(kEndpointTolerance, kEndpointTolerance, kEndpointTolerance);
		const Vec3 maxs = targetOrigin + targetMaxs - projectileMins + Vec3(kEndpointTolerance, kEndpointTolerance, kEndpointTolerance);

		return point.x >= mins.x && point.x <= maxs.x
			&& point.y >= mins.y && point.y <= maxs.y
			&& point.z >= mins.z && point.z <= maxs.z;
	}

	Vec3 ClosestPointOnBounds(const Vec3& point, const Vec3& origin, const Vec3& mins, const Vec3& maxs)
	{
		const Vec3 worldMins = origin + mins;
		const Vec3 worldMaxs = origin + maxs;
		return {
			std::clamp(point.x, worldMins.x, worldMaxs.x),
			std::clamp(point.y, worldMins.y, worldMaxs.y),
			std::clamp(point.z, worldMins.z, worldMaxs.z)
		};
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

bool CAimbotProjectile::GetProjectileInfo(C_TFWeaponBase* pWeapon)
{
	m_CurProjInfo = {};
	const auto pLocal = H::Entities->GetLocal();
	if (!pLocal || !pWeapon)
		return false;

	ProjectileInfo info{};
	if (!F::ProjectileSim->GetInfo(pLocal, pWeapon, I::EngineClient->GetViewAngles(), info))
		return false;

	m_CurProjInfo.Speed = info.m_speed;
	m_CurProjInfo.GravityMod = info.m_gravity_mod;
	m_CurProjInfo.Pipes = IsVPhysicsProjectile(info.m_type);
	m_CurProjInfo.Flamethrower = pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER;
	return m_CurProjInfo.Speed > 0.0f;
}

bool CAimbotProjectile::SolveProjectile(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vTo,
	float flSpeed, float flGravityMod, Vec3& vViewAngleOut, float& flTimeOut, ProjectileInfo& launchOut)
{
	if (!pLocal || !pWeapon || flSpeed <= 0.0f)
		return false;

	Vec3 vViewAngle = Math::CalcAngle(pLocal->GetShootPos(), vTo);
	for (int nIteration = 0; nIteration < 8; nIteration++)
	{
		ProjectileInfo launch{};
		if (!F::ProjectileSim->GetInfo(pLocal, pWeapon, vViewAngle, launch))
			return false;

		launch.m_speed = flSpeed;
		launch.m_gravity_mod = flGravityMod;

		BallisticSolver::SolverParams params;
		params.ShootPos = launch.m_pos;
		params.TargetPos = vTo;
		params.TargetVel = {};
		params.Speed = flSpeed;
		params.Gravity = GetProjectileGravity(flGravityMod);
		params.MuzzleUpZ = GetMuzzleUpZ(pWeapon);
		params.UseViewUpMuzzle = true;
		params.UseHighArc = CFG::Aimbot_Projectile_High_Arc;
		params.DragCoeff = BallisticSolver::ComputeDragCoefficient(GetWeaponDragClass(pWeapon));
		params.DragIters = 3;

		const BallisticSolver::SolveResult result = BallisticSolver::SolveBallistic(params);
		if (!result.Valid || !IsWithinSimTimeLimit(pWeapon, result.Time))
			return false;

		const Vec3 vRequiredLaunchAngle = BallisticSolver::DirectionToAngles(result.Direction);
		Vec3 vCorrection = vRequiredLaunchAngle - launch.m_ang;
		vCorrection.x = Math::NormalizeAngle(vCorrection.x);
		vCorrection.y = Math::NormalizeAngle(vCorrection.y);
		vCorrection.z = 0.0f;

		if (fabsf(vCorrection.x) <= 0.01f && fabsf(vCorrection.y) <= 0.01f)
		{
			vViewAngleOut = vViewAngle;
			flTimeOut = result.Time;
			launchOut = launch;
			return true;
		}

		vViewAngle += vCorrection;
		Math::ClampAngles(vViewAngle);
	}

	return false;
}

CAimbotProjectile::PredictedTargetState_t CAimbotProjectile::CaptureTargetState(C_TFPlayer* pPlayer,
	const Vec3& vOrigin, float flModelScale, const Vec3& vHeadOffset) const
{
	PredictedTargetState_t state{};
	state.Origin = vOrigin;
	state.Mins = pPlayer->m_vecMins();
	state.Maxs = pPlayer->m_vecMaxs();
	state.HeadOffset = vHeadOffset;
	state.ModelScale = flModelScale;
	state.Ducked = pPlayer->m_bDucked() || (pPlayer->m_fFlags() & FL_DUCKING);
	state.OnGround = (pPlayer->m_fFlags() & FL_ONGROUND) || pPlayer->m_hGroundEntity().Get();
	return state;
}

CAimbotProjectile::PredictedTargetState_t CAimbotProjectile::GetTargetStateAtTime(float flTime) const
{
	PredictedTargetState_t state{};
	if (m_TargetStates.empty())
		return state;

	const float flPathIndex = std::clamp(flTime / TICK_INTERVAL, 0.0f,
		static_cast<float>(m_TargetStates.size() - 1));
	const int nLower = static_cast<int>(floorf(flPathIndex));
	const int nUpper = std::min(nLower + 1, static_cast<int>(m_TargetStates.size()) - 1);
	const float flFraction = flPathIndex - static_cast<float>(nLower);
	const PredictedTargetState_t& lower = m_TargetStates[nLower];
	const PredictedTargetState_t& upper = m_TargetStates[nUpper];

	state = flFraction < 0.5f ? lower : upper;
	state.Origin = lower.Origin + (upper.Origin - lower.Origin) * flFraction;
	state.Mins = lower.Mins + (upper.Mins - lower.Mins) * flFraction;
	state.Maxs = lower.Maxs + (upper.Maxs - lower.Maxs) * flFraction;
	state.HeadOffset = lower.HeadOffset + (upper.HeadOffset - lower.HeadOffset) * flFraction;
	state.ModelScale = lower.ModelScale + (upper.ModelScale - lower.ModelScale) * flFraction;
	return state;
}

Vec3 CAimbotProjectile::GetAimPoint(C_TFWeaponBase* pWeapon, const PredictedTargetState_t& state, int aimPosition)
{
	Vec3 vPos = state.Origin;
	float flMinZ = state.Mins.z;
	float flHeight = state.Maxs.z - state.Mins.z;
	if (flHeight <= 1.0f)
	{
		flMinZ = 0.0f;
		flHeight = (state.Ducked ? 62.0f : 82.0f) * state.ModelScale;
	}

	auto applyHeight = [&](float flFraction)
	{
		vPos.z += flMinZ + flHeight * flFraction;
	};

	switch (aimPosition)
	{
		case 0:
		{
			applyHeight(0.2f);
			m_LastAimPos = 0;
			break;
		}
		case 1:
		{
			applyHeight(0.5f);
			m_LastAimPos = 1;
			break;
		}
		case 2:
		{
			if (CFG::Aimbot_Projectile_Advanced_Head_Aim && !state.HeadOffset.IsZero())
			{
				vPos += state.HeadOffset;
			}
			else
			{
				applyHeight(0.85f);
			}
			m_LastAimPos = 2;
			break;
		}
		case 3:
		{
			if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
			{
				if (CFG::Aimbot_Projectile_Advanced_Head_Aim && !state.HeadOffset.IsZero())
				{
					vPos += state.HeadOffset;
				}
				else
				{
					applyHeight(0.92f);
				}
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
						if (state.OnGround)
						{
							applyHeight(0.2f);
							m_LastAimPos = 0;
						}
						else
						{
							applyHeight(0.5f);
							m_LastAimPos = 1;
						}
						break;
					}
					case TF_WEAPON_PIPEBOMBLAUNCHER:
					{
						applyHeight(0.1f);
						m_LastAimPos = 0;
						break;
					}
					default:
					{
						applyHeight(0.5f);
						m_LastAimPos = 1;
						break;
					}
				}
			}
			break;
		}
		default: break;
	}

	return vPos;
}

bool CAimbotProjectile::CanArcReach(const ProjectileInfo& launch, float flTargetTime,
	const PredictedTargetState_t& targetState, C_BaseEntity* pTarget)
{
	if (flTargetTime <= 0.0f)
		return false;

	// Pipe-family solvers include the game's +200 view-up launch impulse, so the
	// verification simulation must initialize with that same impulse enabled.
	if (!F::ProjectileSim->Init(launch))
		return false;

	CTraceFilterArc filter{};
	filter.m_pIgnore = H::Entities->GetLocal();
	filter.m_pIgnore2 = pTarget;

	Vec3 projectileMins{};
	Vec3 projectileMaxs{};
	GetProjectileHull(launch, projectileMins, projectileMaxs);

	float flElapsed = 0.0f;
	Vec3 vImpact = F::ProjectileSim->GetOrigin();
	while (flElapsed + 1e-6f < flTargetTime)
	{
		const Vec3 pre = F::ProjectileSim->GetOrigin();
		F::ProjectileSim->RunTick();
		const Vec3 post = F::ProjectileSim->GetOrigin();
		const float flStep = std::min(TICK_INTERVAL, flTargetTime - flElapsed);
		const float flFraction = std::clamp(flStep / TICK_INTERVAL, 0.0f, 1.0f);
		const Vec3 segmentEnd = pre + (post - pre) * flFraction;

		trace_t trace{};
		H::AimUtils->TraceHull(pre, segmentEnd, projectileMins, projectileMaxs, MASK_SOLID, &filter, &trace);
		if (trace.DidHit() || trace.startsolid || trace.allsolid)
			return false;

		vImpact = segmentEnd;
		flElapsed += flStep;
	}

	return IsInsideExpandedBounds(vImpact, targetState.Origin, targetState.Mins, targetState.Maxs,
		projectileMins, projectileMaxs);
}

bool CAimbotProjectile::CanSee(const ProjectileInfo& launch, const PredictedTargetState_t& targetState,
	C_BaseEntity* pTarget, float flTargetTime)
{
	return CanArcReach(launch, flTargetTime, targetState, pTarget);
}

bool CAimbotProjectile::RunSplash(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd,
                                  const Vec3& vLocalPos, const Vec3& center, ProjTarget_t& target)
{
	const auto isRocketLauncher{ pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER };
	const auto isParticleCannon{ pWeapon->GetWeaponID() == TF_WEAPON_PARTICLE_CANNON };
	const auto isDirectHit{ pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT };
	const auto isAirStrike{ pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheAirStrike };

	if (!isRocketLauncher && !isParticleCannon && !isDirectHit && !isAirStrike)
		return false;

	const Vec3 vShooterDir = (vLocalPos - center).Normalized();

	const int numPoints = CFG::Aimbot_Projectile_Rocket_Splash == 2 ? kRocketSplashPointsReduced : kRocketSplashPointsDefault;
	const Vec3* pSpherePoints = CFG::Aimbot_Projectile_Rocket_Splash == 2
		? GetRocketSplashSpherePoints<kRocketSplashPointsReduced>().data()
		: GetRocketSplashSpherePoints<kRocketSplashPointsDefault>().data();
	constexpr float kRocketExplosionRadius = 146.0f;
	const float radius = SDKUtils::AttribHookValue(kRocketExplosionRadius, "mult_explosion_radius", pWeapon);
	if (radius <= 0.0f)
		return false;

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
	CTraceFilterArc filterVal{};
	filterVal.m_pIgnore = pLocal;
	filterVal.m_pIgnore2 = target.Entity;
	(void)pCmd;
	const bool bUsesPlannedCharge = target.PlannedSpeed > 0.0f;
	const float flProjectileSpeed = bUsesPlannedCharge ? target.PlannedSpeed : m_CurProjInfo.Speed;
	const float flGravityMod = bUsesPlannedCharge ? target.PlannedGravityMod : m_CurProjInfo.GravityMod;
	float flRemainingChargeWait = 0.0f;
	if (bUsesPlannedCharge && target.RequiredChargeTime > 0.0f)
	{
		const float flChargeBegin = pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime();
		const float flCurrentCharge = flChargeBegin > 0.0f
			? static_cast<float>(pLocal->m_nTickBase()) * TICK_INTERVAL - flChargeBegin
			: 0.0f;
		flRemainingChargeWait = std::max(0.0f, target.RequiredChargeTime - flCurrentCharge);
	}
	const float flTimingBias = GetPredictionTimingBias(pWeapon);

	for (std::size_t n = 0; n < potentialCount; n++)
	{
		const Vec3& point = potential[n].Position;

		Vec3 vCandidateAngle{};
		float flCandidateTime = 0.0f;
		ProjectileInfo launch{};
		if (!SolveProjectile(pLocal, pWeapon, point, flProjectileSpeed, flGravityMod,
			vCandidateAngle, flCandidateTime, launch))
			continue;

		Vec3 projectileMins{};
		Vec3 projectileMaxs{};
		GetProjectileHull(launch, projectileMins, projectileMaxs);
		const Vec3 vTraceDirection = (point - launch.m_pos).Normalized();
		const float flTraceExtension = std::max({
			fabsf(projectileMins.x), fabsf(projectileMins.y), fabsf(projectileMins.z),
			fabsf(projectileMaxs.x), fabsf(projectileMaxs.y), fabsf(projectileMaxs.z)
		}) + 2.0f;
		traceVal = {};

		H::AimUtils->TraceHull
		(
			launch.m_pos,
			point + vTraceDirection * flTraceExtension,
			projectileMins,
			projectileMaxs,
			MASK_SOLID,
			&filterVal,
			&traceVal
		);

		if (!traceVal.DidHit() || traceVal.startsolid || traceVal.allsolid)
			continue;

		const Vec3 vImpact = traceVal.endpos;
		const float flPointDistance = launch.m_pos.DistTo(point);
		if (flPointDistance <= 1e-4f)
			continue;
		const float flImpactDistance = launch.m_pos.DistTo(vImpact);
		const float flImpactTime = flCandidateTime * std::clamp(flImpactDistance / flPointDistance, 0.0f, 1.0f);

		PredictedTargetState_t predictedState{};
		const float flTargetTime = flTimingBias + flRemainingChargeWait + flImpactTime;
		if (target.Entity->GetClassId() == ETFClassIds::CTFPlayer)
		{
			if (m_TargetStates.empty())
				continue;
			const float flPathHorizon = static_cast<float>(m_TargetStates.size() - 1) * TICK_INTERVAL;
			if (flTargetTime < 0.0f || flTargetTime > flPathHorizon)
				continue;
			predictedState = GetTargetStateAtTime(flTargetTime);
		}
		else
		{
			predictedState.Origin = target.Entity->m_vecOrigin();
			predictedState.Mins = target.Entity->m_vecMins();
			predictedState.Maxs = target.Entity->m_vecMaxs();
		}

		const Vec3 vTargetPoint = ClosestPointOnBounds(
			vImpact, predictedState.Origin, predictedState.Mins, predictedState.Maxs);
		if (vImpact.DistTo(vTargetPoint) > radius)
			continue;

		trace_t visibilityTrace{};
		CTraceFilterWorldCustom visibilityFilter{};
		const Vec3 vVisibilityStart = vImpact + traceVal.plane.normal * 1.0f;
		H::AimUtils->Trace(vVisibilityStart, vTargetPoint, MASK_SOLID, &visibilityFilter, &visibilityTrace);
		if (visibilityTrace.DidHit() || visibilityTrace.startsolid || visibilityTrace.allsolid)
			continue;

		target.AngleTo = vCandidateAngle;
		target.TimeToTarget = flImpactTime;
		return true;
	}

	return false;
}

bool CAimbotProjectile::SolveTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd, ProjTarget_t& target)
{
	const Vec3 vLocalPos = pLocal->GetShootPos();
	m_TargetPath.clear();
	m_TargetStates.clear();

	const float gravity   = GetProjectileGravity(m_CurProjInfo.GravityMod);
	const float muzzleUpZ = GetMuzzleUpZ(pWeapon);
	const float dragCoeff = BallisticSolver::ComputeDragCoefficient(GetWeaponDragClass(pWeapon));
	const bool  useHighArc = CFG::Aimbot_Projectile_High_Arc;
	const float flTimingBias = GetPredictionTimingBias(pWeapon);

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

		const Vec3 targetVel = pPlayer->m_vecVelocity();
		const float flTargetModelScale = pPlayer->m_flModelScale();
		Vec3 vHeadOffset{};
		if (CFG::Aimbot_Projectile_Advanced_Head_Aim)
		{
			const Vec3 vHeadPosition = pPlayer->GetHitboxPos(HITBOX_HEAD);
			if (!vHeadPosition.IsZero())
				vHeadOffset = vHeadPosition - pPlayer->m_vecOrigin();
		}
		const PredictedTargetState_t currentState = CaptureTargetState(
			pPlayer, pPlayer->m_vecOrigin(), flTargetModelScale, vHeadOffset);
		const Vec3 vCurrentAimPoint = GetAimPoint(pWeapon, currentState, CFG::Aimbot_Projectile_Aim_Position);

		ProjectileInfo seedLaunch{};
		if (!F::ProjectileSim->GetInfo(pLocal, pWeapon, pCmd->viewangles, seedLaunch))
			return false;
		seedLaunch.m_speed = m_CurProjInfo.Speed;
		seedLaunch.m_gravity_mod = m_CurProjInfo.GravityMod;

		BallisticSolver::SolverParams initParams;
		initParams.ShootPos   = seedLaunch.m_pos;
		initParams.TargetPos  = vCurrentAimPoint;
		initParams.TargetVel  = targetVel;
		initParams.Speed      = m_CurProjInfo.Speed;
		initParams.Gravity    = gravity;
		initParams.MuzzleUpZ  = muzzleUpZ;
		initParams.UseViewUpMuzzle = true;
		initParams.UseHighArc = useHighArc;
		initParams.DragCoeff   = dragCoeff;
		initParams.DragIters   = 3;

		BallisticSolver::SolveResult initResult = BallisticSolver::SolveBallistic(initParams);
		const float flMaxSimulationTime = std::max(TICK_INTERVAL, CFG::Aimbot_Projectile_Max_Simulation_Time);
		const int hardMaxTicks = std::max(1, TIME_TO_TICKS(flMaxSimulationTime));

		CMovementSimScope simScope(pPlayer);
		if (!simScope)
			return false;

		m_TargetPath.reserve(hardMaxTicks + 1);
		m_TargetStates.reserve(hardMaxTicks + 1);
		for (int nTick = 0; nTick <= hardMaxTicks; nTick++)
		{
			const PredictedTargetState_t state = CaptureTargetState(
				pPlayer, F::MovementSimulation->GetOrigin(), flTargetModelScale, vHeadOffset);
			m_TargetStates.push_back(state);
			m_TargetPath.push_back(state.Origin);
			if (nTick < hardMaxTicks)
				F::MovementSimulation->RunTick(TICKS_TO_TIME(nTick));
		}

		if (m_TargetPath.empty())
			return false;

		auto buildAimPath = [&](int aimPosition)
		{
			std::vector<Vec3> path;
			path.reserve(m_TargetStates.size());
			for (const PredictedTargetState_t& state : m_TargetStates)
				path.push_back(GetAimPoint(pWeapon, state, aimPosition));
			return path;
		};

		std::vector<Vec3> aimPath = buildAimPath(CFG::Aimbot_Projectile_Aim_Position);
		const int baselineAimPosition = m_LastAimPos;

		auto refinePath = [&]()
		{
			int startTick = -1;
			if (initResult.Valid && initResult.Time > 0.0f)
			{
				startTick = std::clamp(TIME_TO_TICKS(initResult.Time + flTimingBias), 0, static_cast<int>(m_TargetPath.size()) - 1);
			}
			else
			{
				startTick = BallisticSolver::ScanMeetingTick(
					aimPath, TICK_INTERVAL, seedLaunch.m_pos, m_CurProjInfo.Speed,
					gravity, muzzleUpZ, dragCoeff, flMaxSimulationTime, useHighArc, flTimingBias);
			}

			return BallisticSolver::NewtonRefineOverPath(
				aimPath, TICK_INTERVAL, startTick, seedLaunch.m_pos,
				m_CurProjInfo.Speed, gravity, muzzleUpZ, dragCoeff, useHighArc, 3, flTimingBias);
		};

		BallisticSolver::NewtonRefineResult refineResult = refinePath();

		if (!refineResult.Valid || refineResult.HorizonLimited || refineResult.AtPathBoundary)
			return false;

		int meetingTick = std::clamp(refineResult.Tick, 0, static_cast<int>(m_TargetPath.size()) - 1);
		Vec3 vTarget = refineResult.AimPoint;
		PredictedTargetState_t impactState = m_TargetStates[meetingTick];
		impactState.Origin += vTarget - aimPath[meetingTick];

		ProjectileInfo baselineLaunch{};
		if (!SolveProjectile(pLocal, pWeapon, vTarget, m_CurProjInfo.Speed, m_CurProjInfo.GravityMod,
			target.AngleTo, target.TimeToTarget, baselineLaunch))
			return false;

		// The true muzzle origin is angle-dependent. Refine once more with the exact
		// launch origin, then solve the view/launch fixed point for that aim point.
		refineResult = BallisticSolver::NewtonRefineOverPath(
			aimPath, TICK_INTERVAL, meetingTick, baselineLaunch.m_pos,
			m_CurProjInfo.Speed, gravity, muzzleUpZ, dragCoeff, useHighArc, 3, flTimingBias);
		if (!refineResult.Valid || refineResult.HorizonLimited || refineResult.AtPathBoundary)
			return false;

		meetingTick = std::clamp(refineResult.Tick, 0, static_cast<int>(m_TargetStates.size()) - 1);
		vTarget = refineResult.AimPoint;
		impactState = m_TargetStates[meetingTick];
		impactState.Origin += vTarget - aimPath[meetingTick];
		if (!SolveProjectile(pLocal, pWeapon, vTarget, m_CurProjInfo.Speed, m_CurProjInfo.GravityMod,
			target.AngleTo, target.TimeToTarget, baselineLaunch))
			return false;
		if (fabsf(refineResult.SimulatedTime - (target.TimeToTarget + flTimingBias)) > TICK_INTERVAL * 0.5f)
			return false;

		const int baselineMeetingTick = meetingTick;
		const Vec3 baselineTarget = vTarget;
		const PredictedTargetState_t baselineImpactState = impactState;
		const Vec3 baselineAngle = target.AngleTo;
		const float baselineTime = target.TimeToTarget;

		ProjectileInfo selectedLaunch = baselineLaunch;
		const int nChargeWeaponID = pWeapon->GetWeaponID();
		if (CFG::Aimbot_AutoShoot && CFG::Aimbot_Projectile_Charge_Shot
			&& (nChargeWeaponID == TF_WEAPON_COMPOUND_BOW || nChargeWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER))
		{
			const bool bIsBow = nChargeWeaponID == TF_WEAPON_COMPOUND_BOW;
			const float flSpeedMin = bIsBow ? 1800.0f : 900.0f;
			const float flSpeedMax = bIsBow ? 2600.0f : 2400.0f;
			const float flChargeRate = bIsBow ? 1.0f : SDKUtils::AttribHookValue(4.0f, "stickybomb_charge_rate", pWeapon);
			const float flChargeBegin = pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime();
			const float flCurCharge = flChargeBegin > 0.0f
				? static_cast<float>(pLocal->m_nTickBase()) * TICK_INTERVAL - flChargeBegin
				: 0.0f;

			auto gravityModFor = [&](float flSpeed)
			{
				return bIsBow ? 0.5f - 0.4f * ((flSpeed - 1800.0f) / 800.0f) : 1.0f;
			};
			auto chargeTimeFor = [&](float flSpeed)
			{
				if (bIsBow)
					return std::clamp((flSpeed - 1800.0f) / 800.0f, 0.0f, 1.0f);
				return std::clamp(flChargeRate * (flSpeed - 900.0f) / 1500.0f, 0.0f, flChargeRate);
			};

			float flBestScore = baselineTime;
			for (int n = 0; n < 8; n++)
			{
				const float flSpeed = flSpeedMin + (flSpeedMax - flSpeedMin) * (static_cast<float>(n) / 7.0f);
				const float flGravityMod = gravityModFor(flSpeed);
				const float flChargeTime = chargeTimeFor(flSpeed);
				if (flChargeTime < flCurCharge - 1e-4f)
					continue;

				const float flWait = std::max(0.0f, flChargeTime - flCurCharge);
				const float flPlannedBias = flTimingBias + flWait;
				BallisticSolver::NewtonRefineResult plannedRefine = BallisticSolver::NewtonRefineOverPath(
					aimPath, TICK_INTERVAL, meetingTick, baselineLaunch.m_pos,
					flSpeed, GetProjectileGravity(flGravityMod), muzzleUpZ, dragCoeff, useHighArc, 3, flPlannedBias);
				if (!plannedRefine.Valid || plannedRefine.HorizonLimited || plannedRefine.AtPathBoundary)
					continue;

				ProjectileInfo plannedLaunch{};
				Vec3 vPlannedAngle{};
				float flPlannedFlight = 0.0f;
				if (!SolveProjectile(pLocal, pWeapon, plannedRefine.AimPoint, flSpeed, flGravityMod,
					vPlannedAngle, flPlannedFlight, plannedLaunch))
					continue;

				plannedRefine = BallisticSolver::NewtonRefineOverPath(
					aimPath, TICK_INTERVAL, plannedRefine.Tick, plannedLaunch.m_pos,
					flSpeed, GetProjectileGravity(flGravityMod), muzzleUpZ, dragCoeff, useHighArc, 3, flPlannedBias);
				if (!plannedRefine.Valid || plannedRefine.HorizonLimited || plannedRefine.AtPathBoundary)
					continue;
				if (!SolveProjectile(pLocal, pWeapon, plannedRefine.AimPoint, flSpeed, flGravityMod,
					vPlannedAngle, flPlannedFlight, plannedLaunch))
					continue;
				if (fabsf(plannedRefine.SimulatedTime - (flPlannedFlight + flPlannedBias)) > TICK_INTERVAL * 0.5f)
					continue;

				const float flScore = flWait + flPlannedFlight;
				if (flScore + 0.05f >= flBestScore)
					continue;

				flBestScore = flScore;
				meetingTick = std::clamp(plannedRefine.Tick, 0, static_cast<int>(m_TargetStates.size()) - 1);
				vTarget = plannedRefine.AimPoint;
				impactState = m_TargetStates[meetingTick];
				impactState.Origin += vTarget - aimPath[meetingTick];
				target.AngleTo = vPlannedAngle;
				target.TimeToTarget = flPlannedFlight;
				target.PlannedSpeed = flSpeed;
				target.PlannedGravityMod = flGravityMod;
				target.RequiredChargeTime = flChargeTime;
				selectedLaunch = plannedLaunch;
			}
		}

		if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
		{
			const auto sticky_arm_time{ SDKUtils::AttribHookValue(0.8f, "sticky_arm_time", pLocal) };
			if (target.TimeToTarget < sticky_arm_time)
				return false;
		}

		if (CFG::Aimbot_Projectile_Rocket_Splash == 2)
		{
			const Vec3 splashCenter = impactState.Origin;
			if (RunSplash(pLocal, pWeapon, pCmd, vLocalPos, splashCenter, target))
				return true;
		}

		if (CanSee(selectedLaunch, impactState, target.Entity, target.TimeToTarget))
			return true;

		// A rejected charged trajectory must restore every baseline field together;
		// retaining its meeting tick was the source of the old fallback over-lead.
		if (target.PlannedSpeed > 0.0f)
		{
			meetingTick = baselineMeetingTick;
			vTarget = baselineTarget;
			impactState = baselineImpactState;
			target.AngleTo = baselineAngle;
			target.TimeToTarget = baselineTime;
			target.PlannedSpeed = 0.0f;
			target.PlannedGravityMod = 0.0f;
			target.RequiredChargeTime = 0.0f;
			selectedLaunch = baselineLaunch;
			if (CanSee(selectedLaunch, impactState, target.Entity, target.TimeToTarget))
				return true;
		}

		if (CFG::Aimbot_Projectile_BBOX_Multipoint && pWeapon->GetWeaponID() != TF_WEAPON_COMPOUND_BOW)
		{
			// multipoint retries use baseline (tap-fire) ballistics, drop any charge plan
			target.PlannedSpeed = 0.0f;
			target.PlannedGravityMod = 0.0f;
			target.RequiredChargeTime = 0.0f;

			for (int n = 0; n < 3; n++)
			{
				if (n == baselineAimPosition)
					continue;

				const std::vector<Vec3> mpPath = buildAimPath(n);
				BallisticSolver::NewtonRefineResult mpRefine = BallisticSolver::NewtonRefineOverPath(
					mpPath, TICK_INTERVAL, baselineMeetingTick, baselineLaunch.m_pos,
					m_CurProjInfo.Speed, gravity, muzzleUpZ, dragCoeff, useHighArc, 3, flTimingBias);
				if (!mpRefine.Valid || mpRefine.HorizonLimited || mpRefine.AtPathBoundary)
					continue;

				ProjectileInfo mpLaunch{};
				if (!SolveProjectile(pLocal, pWeapon, mpRefine.AimPoint, m_CurProjInfo.Speed, m_CurProjInfo.GravityMod,
					target.AngleTo, target.TimeToTarget, mpLaunch))
					continue;

				mpRefine = BallisticSolver::NewtonRefineOverPath(
					mpPath, TICK_INTERVAL, mpRefine.Tick, mpLaunch.m_pos,
					m_CurProjInfo.Speed, gravity, muzzleUpZ, dragCoeff, useHighArc, 3, flTimingBias);
				if (!mpRefine.Valid || mpRefine.HorizonLimited || mpRefine.AtPathBoundary)
					continue;
				if (!SolveProjectile(pLocal, pWeapon, mpRefine.AimPoint, m_CurProjInfo.Speed, m_CurProjInfo.GravityMod,
					target.AngleTo, target.TimeToTarget, mpLaunch))
					continue;
				if (fabsf(mpRefine.SimulatedTime - (target.TimeToTarget + flTimingBias)) > TICK_INTERVAL * 0.5f)
					continue;

				const int mpTick = std::clamp(mpRefine.Tick, 0, static_cast<int>(m_TargetStates.size()) - 1);
				PredictedTargetState_t mpState = m_TargetStates[mpTick];
				mpState.Origin += mpRefine.AimPoint - mpPath[mpTick];
				if (CanSee(mpLaunch, mpState, target.Entity, target.TimeToTarget))
					return true;
			}
		}

		if (CFG::Aimbot_Projectile_Rocket_Splash == 1)
		{
			const Vec3 splashCenter = baselineImpactState.Origin;
			if (RunSplash(pLocal, pWeapon, pCmd, vLocalPos, splashCenter, target))
				return true;
		}
	}
	else
	{
		const Vec3 vTarget = target.Entity->GetCenter();
		ProjectileInfo launch{};
		if (!SolveProjectile(pLocal, pWeapon, vTarget, m_CurProjInfo.Speed, m_CurProjInfo.GravityMod,
			target.AngleTo, target.TimeToTarget, launch))
			return false;

		if (target.TimeToTarget + flTimingBias > CFG::Aimbot_Projectile_Max_Simulation_Time + TICK_INTERVAL * 0.5f)
			return false;

		if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
		{
			if (target.TimeToTarget < SDKUtils::AttribHookValue(0.8f, "sticky_arm_time", pLocal))
				return false;
		}

		PredictedTargetState_t targetState{};
		targetState.Origin = target.Entity->m_vecOrigin();
		targetState.Mins = target.Entity->m_vecMins();
		targetState.Maxs = target.Entity->m_vecMaxs();
		if (CanSee(launch, targetState, target.Entity, target.TimeToTarget))
			return true;

		if (CFG::Aimbot_Projectile_Rocket_Splash && RunSplash(pLocal, pWeapon, pCmd, vLocalPos, vTarget, target))
			return true;
	}

	m_TargetPath.clear();
	m_TargetStates.clear();
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
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = vLocalPos.DistTo(vPos);

			// The cone is a hard constraint in every sort mode - sort only orders
			// candidates that already passed it
			if (flFOVTo > flFOVLimit)
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
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = vLocalPos.DistTo(vPos);

			// Same hard cone constraint as the player loop
			if (flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(AimTarget_t { pBuilding, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	if (m_vecTargets.empty())
		return false;

	F::AimbotCommon->Sort(m_vecTargets, nSortMode);

	const auto maxTargets{ std::min(CFG::Aimbot_Projectile_Max_Processing_Targets, static_cast<int>(m_vecTargets.size())) };
	auto targetsScanned{ 0 };

	for (auto& target : m_vecTargets)
	{
		if (targetsScanned >= maxTargets)
			break;

		targetsScanned++;

		if (!SolveTarget(pLocal, pWeapon, pCmd, target))
			continue;

		// Re-test the cone against the solved angle, which carries the arc loft and
		// target lead the pre-solve test could not know about
		if (Math::CalcFov(vLocalAngles, target.AngleTo) > flFOVLimit)
			continue;

		outTarget = target;
		return true;
	}

	return false;
}

bool CAimbotProjectile::ShouldAim(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	const bool bChargeRelease = m_ChargeHold.ReleasePending && pCmd
		&& pCmd->command_number == m_ChargeHold.LastSolvedCommandNumber;
	return CFG::Aimbot_Projectile_Aim_Type != 1 || bChargeRelease
		|| IsFiring(pCmd, pLocal, pWeapon) && pWeapon->HasPrimaryAmmoForShot();
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
			const bool bChargeRelease = m_ChargeHold.ReleasePending && pCmd
				&& pCmd->command_number == m_ChargeHold.LastSolvedCommandNumber;
			if (m_CurProjInfo.Flamethrower || G::bCanPrimaryAttack || bChargeRelease)
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

void CAimbotProjectile::ResetChargeHold()
{
	m_ChargeHold = {};
}

bool CAimbotProjectile::MaintainChargeHold(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (!m_ChargeHold.Active)
		return false;

	if (!pCmd)
		return true;

	const int nWeaponID = pWeapon ? pWeapon->GetWeaponID() : TF_WEAPON_NONE;
	const bool bValidChargeWeapon = nWeaponID == TF_WEAPON_COMPOUND_BOW
		|| nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER;
	const bool bSwitchingWeapon = pCmd->weaponselect != 0
		&& (!pWeapon || pCmd->weaponselect != pWeapon->entindex());
	const bool bCanMaintain = pLocal && !pLocal->deadflag() && pWeapon
		&& bValidChargeWeapon && !bSwitchingWeapon
		&& m_ChargeHold.Weapon.Get() == pWeapon;

	if (!bCanMaintain)
	{
		if (bSwitchingWeapon)
			pCmd->buttons &= ~IN_ATTACK;
		ResetChargeHold();
		return false;
	}

	const float flChargeBegin = pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime();
	if (flChargeBegin > 0.0f)
		m_ChargeHold.ChargeObserved = true;
	else if (m_ChargeHold.ChargeObserved || !pWeapon->HasPrimaryAmmoForShot())
	{
		// The server already fired/cancelled the charge, or the weapon can no
		// longer sustain it. Do not turn a stale state into a new blind charge.
		ResetChargeHold();
		return false;
	}

	const bool bPlayerBlocked = pLocal->InCond(TF_COND_TAUNTING) || pLocal->InCond(TF_COND_PHASE)
		|| pLocal->InCond(TF_COND_HALLOWEEN_GHOST_MODE) || pLocal->InCond(TF_COND_HALLOWEEN_BOMB_HEAD)
		|| pLocal->InCond(TF_COND_HALLOWEEN_KART) || pLocal->m_bFeignDeathReady()
		|| pLocal->m_flInvisibility() > 0.0f;
	const bool bContextStopped = !CFG::Aimbot_Active || !CFG::Aimbot_Projectile_Active
		|| !CFG::Aimbot_AutoShoot || !CFG::Aimbot_Projectile_Charge_Shot
		|| !H::Input->IsDown(CFG::Aimbot_Key)
		|| I::EngineVGui->IsGameUIVisible() || I::MatSystemSurface->IsCursorVisible()
		|| SDKUtils::BInEndOfMatch() || bPlayerBlocked;
	if (bContextStopped)
	{
		QueueChargeRelease(pCmd, true);
		return false;
	}

	// A transient target/prediction failure is not a release signal. Keep the
	// charge until a fresh solve can decide the release command.
	pCmd->buttons |= IN_ATTACK;
	return true;
}

void CAimbotProjectile::RunChargeLifecycle(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (m_ChargeHold.ReleasePending)
	{
		if (!pCmd || pCmd->command_number != m_ChargeHold.LastSolvedCommandNumber)
			ResetChargeHold();
		return;
	}

	MaintainChargeHold(pCmd, pLocal, pWeapon);
}

void CAimbotProjectile::QueueChargeRelease(CUserCmd* pCmd, bool bAbortRelease)
{
	pCmd->buttons &= ~IN_ATTACK;
	if (bAbortRelease)
		m_ChargeHold.LastSolvedCommandNumber = pCmd->command_number;
	m_ChargeHold.Active = false;
	m_ChargeHold.ReleasePending = true;
	m_ChargeHold.AbortRelease = bAbortRelease;
}

void CAimbotProjectile::FinalizeChargeCommand(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, bool bConsume)
{
	if (!m_ChargeHold.ReleasePending || !pCmd
		|| pCmd->command_number != m_ChargeHold.LastSolvedCommandNumber)
		return;

	const bool bSwitchingWeapon = pCmd->weaponselect != 0
		&& (!pWeapon || pCmd->weaponselect != pWeapon->entindex());
	const bool bCurrentSolution = pLocal && !pLocal->deadflag() && pWeapon && !bSwitchingWeapon
		&& m_ChargeHold.Weapon.Get() == pWeapon
		&& (m_ChargeHold.AbortRelease || m_ChargeHold.Target.Get());

	if (bCurrentSolution)
	{
		pCmd->buttons &= ~IN_ATTACK;
		Aim(pCmd, pLocal, pWeapon, m_ChargeHold.LastSolvedAngle);
		if (bConsume)
			ResetChargeHold();
		return;
	}

	if (pLocal && !pLocal->deadflag() && pWeapon && !bSwitchingWeapon
		&& m_ChargeHold.Weapon.Get() == pWeapon)
	{
		// The solved target disappeared later in this command. Preserve the hold
		// and let the next prediction pass either rebind or keep it loss-safe.
		m_ChargeHold.Active = true;
		m_ChargeHold.ReleasePending = false;
		pCmd->buttons |= IN_ATTACK;
		return;
	}

	ResetChargeHold();
}

void CAimbotProjectile::HandleFire(CUserCmd* pCmd, C_TFWeaponBase* pWeapon, C_TFPlayer* pLocal, const ProjTarget_t& target)
{
	const bool bIsBazooka = pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka;
	if (!bIsBazooka && !pWeapon->HasPrimaryAmmoForShot())
	{
		ResetChargeHold();
		return;
	}

	const int nWeaponID = pWeapon->GetWeaponID();
	if (nWeaponID == TF_WEAPON_COMPOUND_BOW || nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER)
	{
		const float flChargeBegin = pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime();
		m_ChargeHold.Active = true;
		m_ChargeHold.ChargeObserved = flChargeBegin > 0.0f;
		m_ChargeHold.ReleasePending = false;
		m_ChargeHold.AbortRelease = false;
		m_ChargeHold.Weapon = pWeapon;
		m_ChargeHold.Target = target.Entity;
		m_ChargeHold.LastSolvedAngle = target.AngleTo;
		m_ChargeHold.RequiredChargeTime = target.RequiredChargeTime;
		m_ChargeHold.LastSolvedCommandNumber = pCmd->command_number;

		if (target.RequiredChargeTime > 0.0f && CFG::Aimbot_Projectile_Charge_Shot)
		{
			// planned charged shot: hold until the required charge is reached, then release
			if (flChargeBegin <= 0.0f)
			{
				pCmd->buttons |= IN_ATTACK;
			}
			else
			{
				const float flCharge = (static_cast<float>(pLocal->m_nTickBase()) * TICK_INTERVAL) - flChargeBegin;
				if (flCharge + TICK_INTERVAL >= target.RequiredChargeTime)
					QueueChargeRelease(pCmd);
				else
					pCmd->buttons |= IN_ATTACK;
			}
		}
		else
		{
			if (flChargeBegin > 0.0f)
				QueueChargeRelease(pCmd);
			else
				pCmd->buttons |= IN_ATTACK;
		}
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
	RunChargeLifecycle(pCmd, pLocal, pWeapon);

	if (!CFG::Aimbot_Projectile_Active)
		return;

	if (!GetProjectileInfo(pWeapon))
		return;

	// The cone constrains both sort modes now, so the indicator applies to both
	G::flAimbotFOV = CFG::Aimbot_Projectile_FOV;

	if (Shifting::bShifting && !Shifting::bShiftingWarp)
		return;

	// A release already committed to this command number must be allowed to finish -
	// the delay gates below would otherwise drop its solved angle and leave the
	// charge waiting on a release that never lands.
	const bool bChargeReleaseCommitted = m_ChargeHold.ReleasePending && pCmd
		&& pCmd->command_number == m_ChargeHold.LastSolvedCommandNumber;

	// Delay check - prevents snap aiming. The delay governs aimbot-initiated fire
	// only, so flag it for the triggerbot. A charge already in flight keeps being
	// held by RunChargeLifecycle above and resolves once the window closes.
	if (CFG::Aimbot_Projectile_Delay_Fire && !bChargeReleaseCommitted
		&& I::GlobalVars->curtime < m_flDelayFireEndTime)
	{
		G::bAimbotFireDelayed = true;
		return;
	}

	if (!H::Input->IsDown(CFG::Aimbot_Key))
		return;

	ProjTarget_t target = {};
	if (GetTarget(pLocal, pWeapon, pCmd, target) && target.Entity)
	{
		const int nTargetIndex = target.Entity->entindex();

		// Target switch settle - crossing the fire delay must not license an instant
		// snap onto a *different* target, which in Silent mode is a full view jump.
		// Re-acquiring the same target is unaffected.
		if (CFG::Aimbot_Projectile_Delay_Fire && CFG::Aimbot_Projectile_Delay_Fire_Switch_Time > 0.0f
			&& !bChargeReleaseCommitted && m_nLastFiredTargetIndex > 0 && nTargetIndex != m_nLastFiredTargetIndex
			&& I::GlobalVars->curtime < m_flTargetSwitchEndTime)
		{
			G::bAimbotFireDelayed = true;
			return;
		}

		G::nTargetIndexEarly = nTargetIndex;
		G::nTargetIndex = nTargetIndex;

		if (ShouldFire(pCmd, pLocal, pWeapon))
			HandleFire(pCmd, pWeapon, pLocal, target);

		const bool bIsFiring = IsFiring(pCmd, pLocal, pWeapon);

		G::bFiring = bIsFiring;

		// Reset delay timer after firing
		if (CFG::Aimbot_Projectile_Delay_Fire && bIsFiring)
		{
			m_flDelayFireEndTime = I::GlobalVars->curtime + CFG::Aimbot_Projectile_Delay_Fire_Time;

			// Record who we shot so a later switch to a different target has to
			// settle past the fire delay before it can be acquired
			m_nLastFiredTargetIndex = nTargetIndex;
			m_flTargetSwitchEndTime = m_flDelayFireEndTime + CFG::Aimbot_Projectile_Delay_Fire_Switch_Time;
		}

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
	else
	{
		MaintainChargeHold(pCmd, pLocal, pWeapon);
	}
}
