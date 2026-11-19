#include "AimbotProjectile.h"

#include "AimbotProjectilePrediction.h"
#include "../../CFG.h"
#include "../../MovementSimulation/MovementSimulation.h"
#include "../../ProjectileSim/ProjectileSim.h"
#include <cstring>

namespace
{
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



	float GetPredictionTimingBias(C_TFWeaponBase* pWeapon, bool bCommandChoked)
	{
		const bool bUsesPseudoSilent = CFG::Aimbot_Projectile_Aim_Type == 1
			&& pWeapon && pWeapon->GetWeaponID() != TF_WEAPON_FLAMETHROWER;
		const float flPacketDelay = bUsesPseudoSilent && bCommandChoked ? TICK_INTERVAL : 0.0f;
		// The visible target is interpolated in both modes; cross that delay
		// as well as the outgoing packet latency when predicting its position.
		const float flLerp = SDKUtils::GetLerp();
		return ProjectilePredictionMath::ComputeTimingBias(SDKUtils::GetOutgoingLatency(), flLerp) + flPacketDelay;
	}

	float GetProjectileGravity(float flGravityMod)
	{
		const float flGravity = SDKUtils::GetGravity();
		return (flGravity > 0.0f ? flGravity : 800.0f) * flGravityMod;
	}

	void GetProjectileHull(const ProjectileInfo& info, Vec3& mins, Vec3& maxs)
	{
		mins = info.m_profile.HullMins;
		maxs = info.m_profile.HullMaxs;
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

bool CAimbotProjectile::SeedCandidate(C_TFPlayer* local, C_TFWeaponBase* weapon, ProjectileCandidate_t& candidate)
{
	if (!candidate.Path || candidate.Path->size() < 2 || !(candidate.Speed > 0.0f)) return false;
	const float horizon = std::min(CFG::Aimbot_Projectile_Max_Simulation_Time,
		static_cast<float>(candidate.Path->size() - 1) * TICK_INTERVAL);
	if (!(candidate.TimingBias < horizon)) return false;
	const Vec3 target = ProjectileTrajectory::SamplePath(*candidate.Path, TICK_INTERVAL, candidate.TimingBias);
	candidate.Angle = Math::CalcAngle(local->GetShootPos(), target);
	if (!F::ProjectileSim->GetInfo(local, weapon, candidate.Angle, candidate.Launch)) return false;
	candidate.Launch.m_speed = candidate.Speed;
	candidate.Launch.m_gravity_mod = candidate.GravityMod;
	BallisticSolver::SolverParams params;
	params.ShootPos = candidate.Launch.m_pos;
	params.TargetPos = target;
	params.TargetVel = ((*candidate.Path)[1] - (*candidate.Path)[0]) / TICK_INTERVAL;
	params.Speed = candidate.Speed;
	params.Gravity = GetProjectileGravity(candidate.GravityMod);
	params.MuzzleUpZ = candidate.Launch.m_profile.MuzzleUp;
	params.UseViewUpMuzzle = true;
	params.UseHighArc = candidate.HighArc;
	// Analytic motion is a seed only; native drag is evaluated by VPhysics.
	params.MaxTime = std::min(horizon - candidate.TimingBias, candidate.Launch.m_profile.MaxFlightTime);
	const auto seed = BallisticSolver::SolveBallistic(params);
	if (seed.Valid)
	{
		Math::VectorAngles(seed.Direction, candidate.Angle);
		candidate.FlightTime = seed.Time;
	}
	else
	{
		candidate.FlightTime = std::min(params.MaxTime, target.DistTo(candidate.Launch.m_pos) / candidate.Speed);
	}
	candidate.Score = candidate.WaitTime + candidate.FlightTime + candidate.ArcPenalty;
	return std::isfinite(candidate.FlightTime) && candidate.FlightTime > 0.0f;
}

bool CAimbotProjectile::RefineCandidate(C_TFPlayer* local, C_TFWeaponBase* weapon, ProjectileCandidate_t& candidate)
{
	const float maxFlightTime = std::min(candidate.Launch.m_profile.MaxFlightTime,
		CFG::Aimbot_Projectile_Max_Simulation_Time - candidate.TimingBias);
	auto simulate = [&](const Vec3& angle, float time, Vec3& endpoint)
	{
		ProjectileInfo launch;
		if (!F::ProjectileSim->GetInfo(local, weapon, angle, launch)) return false;
		launch.m_speed = candidate.Speed;
		launch.m_gravity_mod = candidate.GravityMod;
		return F::ProjectileSim->Sample(launch, time, endpoint, &m_TrajectoryBudget);
	};
	const auto result = ProjectileTrajectory::Refine(*candidate.Path, TICK_INTERVAL, candidate.TimingBias,
		maxFlightTime, candidate.Angle, candidate.FlightTime, simulate);
	if (!result.Valid || !F::ProjectileSim->GetInfo(local, weapon, result.Angle, candidate.Launch)) return false;
	candidate.Angle = result.Angle;
	candidate.FlightTime = result.FlightTime;
	candidate.Launch.m_speed = candidate.Speed;
	candidate.Launch.m_gravity_mod = candidate.GravityMod;
	candidate.Score = candidate.WaitTime + candidate.FlightTime + candidate.ArcPenalty;
	return true;
}

bool CAimbotProjectile::SolveProjectile(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vTo,
	float flSpeed, float flGravityMod, Vec3& vViewAngleOut, float& flTimeOut, ProjectileInfo& launchOut)
{
	if (!pLocal || !pWeapon || !(TICK_INTERVAL > 0.0f)) return false;
	const int ticks = std::max(1, static_cast<int>(std::ceil(CFG::Aimbot_Projectile_Max_Simulation_Time / TICK_INTERVAL)));
	std::vector<Vec3> path(static_cast<std::size_t>(ticks + 1), vTo);
	ProjectileCandidate_t candidate;
	candidate.Path = &path;
	candidate.Speed = flSpeed;
	candidate.GravityMod = flGravityMod;
	candidate.HighArc = CFG::Aimbot_Projectile_High_Arc;
	if (!SeedCandidate(pLocal, pWeapon, candidate) || !RefineCandidate(pLocal, pWeapon, candidate)) return false;
	vViewAngleOut = candidate.Angle;
	flTimeOut = candidate.FlightTime;
	launchOut = candidate.Launch;
	return true;
}

void CAimbotProjectile::CaptureHitboxes(C_TFPlayer* player)
{
	m_TargetHitboxes.clear();
	const auto model = player->GetModel();
	const auto header = model ? I::ModelInfoClient->GetStudiomodel(model) : nullptr;
	const auto set = header ? header->pHitboxSet(player->m_nHitboxSet()) : nullptr;
	matrix3x4_t bones[128] = {};
	if (!set || !player->SetupBones(bones, 128, BONE_USED_BY_HITBOX, I::GlobalVars->curtime)) return;
	const Vec3 origin = player->m_vecOrigin();
	for (int index = 0; index < set->numhitboxes; ++index)
	{
		const auto box = set->pHitbox(index);
		if (!box || box->bone < 0 || box->bone >= 128) continue;
		PredictedHitbox_t hitbox;
		std::memcpy(hitbox.Matrix, bones[box->bone], sizeof(hitbox.Matrix));
		for (int axis = 0; axis < 3; ++axis) hitbox.Matrix[axis][3] -= origin[axis];
		hitbox.Mins = box->bbmin;
		hitbox.Maxs = box->bbmax;
		hitbox.Index = index;
		m_TargetHitboxes.push_back(hitbox);
	}
}

bool CAimbotProjectile::SweepHitboxes(const Vec3& start, const Vec3& end,
	const Vec3& originStart, const Vec3& originEnd, float& fraction, int& hitbox) const
{
	bool found = false;
	fraction = 1.0f;
	for (const auto& box : m_TargetHitboxes)
	{
		auto toLocal = [&](const Vec3& world)
		{
			const Vec3 delta = world - Vec3{ box.Matrix[0][3], box.Matrix[1][3], box.Matrix[2][3] };
			Vec3 local;
			for (int axis = 0; axis < 3; ++axis)
			{
				const Vec3 basis{ box.Matrix[0][axis], box.Matrix[1][axis], box.Matrix[2][axis] };
				local[axis] = basis.LengthSqr() > 1e-8f ? delta.Dot(basis) / basis.LengthSqr() : 0.0f;
			}
			return local;
		};
		float contact = 1.0f;
		if (ProjectileTrajectory::SweepBounds(toLocal(start - originStart), toLocal(end - originEnd),
			{}, {}, box.Mins, box.Maxs, box.Mins, box.Maxs, {}, {}, contact)
			&& (!found || contact < fraction))
		{
			fraction = contact;
			hitbox = box.Index;
			found = true;
		}
	}
	return found;
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
	const PredictedTargetState_t& targetState, C_BaseEntity* pTarget, float timingBias, bool requireHead,
	float* impactTime)
{
	if (!std::isfinite(flTargetTime) || flTargetTime <= 0.0f
		|| !m_TrajectoryBudget.TakeTrial() || !F::ProjectileSim->Init(launch, false, &m_TrajectoryBudget)) return false;
	CTraceFilterArc filter{};
	filter.m_pIgnore = H::Entities->GetLocal();
	filter.m_pIgnore2 = pTarget;
	Vec3 projectileMins{}, projectileMaxs{};
	GetProjectileHull(launch, projectileMins, projectileMaxs);
	float elapsed = 0.0f;
	while (elapsed + 1e-6f < flTargetTime)
	{
		if (!m_TrajectoryBudget.TakeStep()) return false;
		const Vec3 before = F::ProjectileSim->GetOrigin();
		F::ProjectileSim->RunTick();
		const float step = std::min(TICK_INTERVAL, flTargetTime - elapsed);
		const Vec3 after = before + (F::ProjectileSim->GetOrigin() - before) * (step / TICK_INTERVAL);
		trace_t trace{};
		H::AimUtils->TraceHull(before, after, projectileMins, projectileMaxs, MASK_SOLID, &filter, &trace);
		if (trace.startsolid || trace.allsolid) return false;
		const bool movingTarget = pTarget->GetClassId() == ETFClassIds::CTFPlayer && !m_TargetStates.empty();
		const auto startState = movingTarget ? GetTargetStateAtTime(timingBias + elapsed) : targetState;
		const auto endState = movingTarget ? GetTargetStateAtTime(timingBias + elapsed + step) : targetState;
		float contact = 1.0f;
		int hitbox = -1;
		const bool hitTarget = requireHead
			? SweepHitboxes(before, after, startState.Origin, endState.Origin, contact, hitbox)
			: ProjectileTrajectory::SweepBounds(before, after, startState.Origin, endState.Origin,
				startState.Mins, startState.Maxs, endState.Mins, endState.Maxs, projectileMins, projectileMaxs, contact);
		if (hitTarget && contact <= (trace.DidHit() ? trace.fraction : 1.0f))
		{
			if (requireHead && hitbox != HITBOX_HEAD) return false;
			if (impactTime) *impactTime = elapsed + step * contact;
			return true;
		}
		if (trace.DidHit()) return false;
		elapsed += step;
	}
	return false;
}

bool CAimbotProjectile::CanSee(const ProjectileInfo& launch, const PredictedTargetState_t& targetState,
	C_BaseEntity* pTarget, float flTargetTime, float timingBias, bool requireHead, float* impactTime)
{
	return CanArcReach(launch, flTargetTime, targetState, pTarget, timingBias, requireHead, impactTime);
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
	const bool bChargeRelease = m_ChargeHold.ReleasePending && pCmd
		&& pCmd->command_number == m_ChargeHold.LastSolvedCommandNumber;
	const bool bCommandChoked = G::bCanPrimaryAttack || bChargeRelease;
	const float flTimingBias = GetPredictionTimingBias(pWeapon, bCommandChoked);

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
	m_TargetPath.clear();
	m_TargetStates.clear();
	m_TargetHitboxes.clear();
	m_TrajectoryBudget = {};
	target.PlannedSpeed = 0.0f;
	target.PlannedGravityMod = 0.0f;
	target.RequiredChargeTime = 0.0f;
	if (!(TICK_INTERVAL > 0.0f) || !std::isfinite(CFG::Aimbot_Projectile_Max_Simulation_Time)
		|| !(CFG::Aimbot_Projectile_Max_Simulation_Time > TICK_INTERVAL)) return false;
	const bool chargeRelease = m_ChargeHold.ReleasePending && pCmd
		&& pCmd->command_number == m_ChargeHold.LastSolvedCommandNumber;
	const float timingBias = GetPredictionTimingBias(pWeapon, G::bCanPrimaryAttack || chargeRelease);
	const Vec3 localPos = pLocal->GetShootPos();
	const int ticks = std::max(1, static_cast<int>(std::ceil(CFG::Aimbot_Projectile_Max_Simulation_Time / TICK_INTERVAL)));
	const bool isPlayer = target.Entity->GetClassId() == ETFClassIds::CTFPlayer;
	if (isPlayer)
	{
		const auto player = target.Entity->As<C_TFPlayer>();
		if (CFG::Aimbot_Projectile_Hitchance_Enabled
			&& F::MovementSimulation->CalculateHitchance(player, player->m_fFlags() & FL_ONGROUND ? 4 : 3)
				< CFG::Aimbot_Projectile_Hitchance_Minimum / 100.0f) return false;
		const float scale = player->m_flModelScale();
		Vec3 headOffset{};
		if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
		{
			CaptureHitboxes(player);
			for (const auto& box : m_TargetHitboxes)
				if (box.Index == HITBOX_HEAD) Math::VectorTransform((box.Mins + box.Maxs) * 0.5f, box.Matrix, headOffset);
		}
		else if (CFG::Aimbot_Projectile_Advanced_Head_Aim)
		{
			const Vec3 head = player->GetHitboxPos(HITBOX_HEAD);
			if (!head.IsZero()) headOffset = head - player->m_vecOrigin();
		}
		// Restore the real entity before launch setup and trajectory validation.
		{
			CMovementSimScope sim(player);
			if (!sim) return false;
			m_TargetStates.reserve(static_cast<std::size_t>(ticks + 1));
			m_TargetPath.reserve(static_cast<std::size_t>(ticks + 1));
			m_TargetStates.push_back(CaptureTargetState(player, F::MovementSimulation->GetOrigin(), scale, headOffset));
			m_TargetPath.push_back(F::MovementSimulation->GetOrigin());
			for (int tick = 1; tick <= ticks; ++tick)
			{
				F::MovementSimulation->RunTick(tick * TICK_INTERVAL);
				m_TargetStates.push_back(CaptureTargetState(player, F::MovementSimulation->GetOrigin(), scale, headOffset));
				m_TargetPath.push_back(F::MovementSimulation->GetOrigin());
			}
		}
	}
	else
	{
		PredictedTargetState_t state;
		state.Origin = target.Entity->m_vecOrigin();
		state.Mins = target.Entity->m_vecMins();
		state.Maxs = target.Entity->m_vecMaxs();
		m_TargetStates.assign(static_cast<std::size_t>(ticks + 1), state);
		m_TargetPath.assign(static_cast<std::size_t>(ticks + 1), state.Origin);
	}
	std::vector<Vec3> aimPath;
	aimPath.reserve(m_TargetStates.size());
	for (const auto& state : m_TargetStates)
		aimPath.push_back(isPlayer ? GetAimPoint(pWeapon, state, CFG::Aimbot_Projectile_Aim_Position)
			: state.Origin + (state.Mins + state.Maxs) * 0.5f);
	const bool requireHead = pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW && m_LastAimPos == 2;
	const float estimate = localPos.DistTo(aimPath.front()) / std::max(m_CurProjInfo.Speed, 1.0f);
	const float splashTime = std::min(timingBias + estimate, CFG::Aimbot_Projectile_Max_Simulation_Time);
	const Vec3 splashCenter = GetTargetStateAtTime(splashTime).Origin;
	if (CFG::Aimbot_Projectile_Rocket_Splash == 2 && RunSplash(pLocal, pWeapon, pCmd, localPos, splashCenter, target))
		return true;
	std::vector<ProjectileCandidate_t> candidates;
	auto add = [&](std::vector<Vec3>& path, float speed, float gravity, float chargeTime, float waitTime, bool highArc)
	{
		ProjectileCandidate_t candidate;
		candidate.Path = &path;
		candidate.Speed = speed;
		candidate.GravityMod = gravity;
		candidate.ChargeTime = chargeTime;
		candidate.WaitTime = waitTime;
		candidate.TimingBias = timingBias + waitTime;
		candidate.HighArc = highArc;
		candidate.ArcPenalty = highArc == CFG::Aimbot_Projectile_High_Arc
			? 0.0f : CFG::Aimbot_Projectile_Max_Simulation_Time * 2.0f;
		if (SeedCandidate(pLocal, pWeapon, candidate)) candidates.push_back(candidate);
	};
	const bool highArc = CFG::Aimbot_Projectile_High_Arc;
	add(aimPath, m_CurProjInfo.Speed, m_CurProjInfo.GravityMod, 0.0f, 0.0f, highArc);
	if (m_CurProjInfo.GravityMod > 0.0f)
		add(aimPath, m_CurProjInfo.Speed, m_CurProjInfo.GravityMod, 0.0f, 0.0f, !highArc);
	const auto chargeProfile = F::ProjectileSim->GetChargeProfile(pWeapon);
	if (CFG::Aimbot_Projectile_Charge_Shot && chargeProfile.Duration > 0.0f)
	{
		const float begin = pWeapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime();
		const float current = begin > 0.0f ? std::max(0.0f, pLocal->m_nTickBase() * TICK_INTERVAL - begin) : 0.0f;
		for (int sample = 1; sample <= 14; ++sample)
		{
			const float charge = chargeProfile.Duration * sample / 14.0f;
			if (charge <= current + TICK_INTERVAL * 0.5f) continue;
			float speed{}, gravity{};
			chargeProfile.Evaluate(charge, speed, gravity);
			add(aimPath, speed, gravity, charge, charge - current, highArc);
		}
	}
	ProjectileCandidate_t selected;
	auto validate = [&](ProjectileCandidate_t& candidate)
	{
		if (selected.Path && candidate.WaitTime + candidate.ArcPenalty >= selected.Score) return false;
		if (!RefineCandidate(pLocal, pWeapon, candidate)) return false;
		const auto state = GetTargetStateAtTime(candidate.TimingBias + candidate.FlightTime);
		float impact = candidate.FlightTime;
		if (!CanSee(candidate.Launch, state, target.Entity, candidate.FlightTime,
			candidate.TimingBias, requireHead, &impact)) return false;
		if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER
			&& impact < SDKUtils::AttribHookValue(0.8f, "sticky_arm_time", pWeapon)) return false;
		candidate.FlightTime = impact;
		candidate.Score = candidate.WaitTime + impact + candidate.ArcPenalty;
		return true;
	};
	bool found = ProjectileTrajectory::SelectViable(candidates, selected, validate);
	// Multipoints have independent paths and do not require a successful baseline.
	if (!found && CFG::Aimbot_Projectile_BBOX_Multipoint && isPlayer
		&& pWeapon->GetWeaponID() != TF_WEAPON_COMPOUND_BOW)
	{
		std::array<std::vector<Vec3>, 3> multipointPaths;
		candidates.clear();
		for (int point = 0; point < 3; ++point)
		{
			for (const auto& state : m_TargetStates) multipointPaths[point].push_back(GetAimPoint(pWeapon, state, point));
			add(multipointPaths[point], m_CurProjInfo.Speed, m_CurProjInfo.GravityMod, 0.0f, 0.0f, highArc);
		}
		found = ProjectileTrajectory::SelectViable(candidates, selected, validate);
	}
	if (found)
	{
		target.AngleTo = selected.Angle;
		target.TimeToTarget = selected.FlightTime;
		target.RequiredChargeTime = selected.ChargeTime;
		if (selected.ChargeTime > 0.0f)
		{
			target.PlannedSpeed = selected.Speed;
			target.PlannedGravityMod = selected.GravityMod;
		}
		return true;
	}
	if (CFG::Aimbot_Projectile_Rocket_Splash == 1 && RunSplash(pLocal, pWeapon, pCmd, localPos, splashCenter, target))
		return true;
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
	//projectiles fire along the command viewangles without punch, so unlike hitscan the aim angles need no punch adjustment
	Vec3 vAngleTo = vAngles;

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
