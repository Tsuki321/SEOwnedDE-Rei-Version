#include "AimbotMelee.h"

#include "../../CFG.h"

bool CAimbotMelee::CanSee(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, MeleeTarget_t& target)
{
	// Hoist GetShootPos out of the distance check + immediate trace + prediction
	// loop. The vfunc was being called twice before the loop and once per
	// prediction tick (typically 10-15 iterations).
	const Vec3 vShootPos = pLocal->GetShootPos();

	if (vShootPos.DistTo(target.Position) > 600.0f)
		return false;

	auto checkPos = [&](const Vec3& vLocalPos) -> bool
	{
		const auto vToSee = [&]()
		{
			auto vForward = Vec3();
			Math::AngleVectors(target.AngleTo, &vForward);
			return vLocalPos + (vForward * pWeapon->GetSwingRange());
		}();

		CLagRecordScope scope(target.LagRecord);

		const bool bCanSee = H::AimUtils->TraceEntityMelee(target.Entity, vLocalPos, vToSee);

		if (CFG::Aimbot_Melee_Aim_Type == 2 || CFG::Aimbot_Melee_Aim_Type == 3)
		{
			const auto vToHit = [&]()
			{
				auto vForward = Vec3();
				Math::AngleVectors(I::EngineClient->GetViewAngles(), &vForward);
				return vLocalPos + (vForward * pWeapon->GetSwingRange());
			}();

			target.MeleeTraceHit = H::AimUtils->TraceEntityMelee(target.Entity, vLocalPos, vToHit);
		}
		else
		{
			target.MeleeTraceHit = bCanSee;
		}

		return bCanSee;
	};

	if (checkPos(vShootPos))
	{
		return true;
	}

	if (!CFG::Aimbot_Melee_Predict_Swing || pLocal->InCond(TF_COND_SHIELD_CHARGE) || pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
	{
		return false;
	}

	// TODO: move this to movement simulation at some point
	auto extrapolate = [](Vec3& vPos, const Vec3& vVel, float flTime, bool bGravity) -> void
	{
		if (bGravity)
			vPos += (vVel * flTime) - Vec3(0.0f, 0.0f, SDKUtils::GetGravity()) * 0.5f * flTime * flTime;

		else vPos += (vVel * flTime);
	};

	const bool bDoGravity = !(pLocal->m_fFlags() & FL_ONGROUND) && pLocal->GetMoveType() == MOVETYPE_WALK;
	const auto predictAmount = CFG::Aimbot_Melee_Predict_Swing_Amount;
	const auto tickInterval = I::GlobalVars->interval_per_tick;

	// Hoist invariant reads out of the per-tick prediction loop. m_vecVelocity
	// and GetClassId are repeated for every tick of the prediction window
	// (typically 10-15 iterations per CanSee call). vShootPos was hoisted
	// to the top of the function so the loop copies from a register.
	const Vec3 vLocalVelocity = pLocal->m_vecVelocity();
	const ETFClassIds nTargetClass = target.Entity->GetClassId();

	for (float flTime = 0.0f; flTime < predictAmount; flTime += tickInterval)
	{
		Vec3 vLocalPos = vShootPos;

		if (nTargetClass == ETFClassIds::CTFPlayer)
			extrapolate(vLocalPos, vLocalVelocity + (target.Entity->As<C_TFPlayer>()->m_vecVelocity() * -1.0f), flTime, bDoGravity);

		else if (target.LagRecord)
			extrapolate(vLocalPos, vLocalVelocity + (target.LagRecord->Velocity * -1.0f), flTime, bDoGravity);

		else extrapolate(vLocalPos, vLocalVelocity, flTime, bDoGravity);

		if (checkPos(vLocalPos))
			return true;
	}

	return false;
}

bool CAimbotMelee::GetTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, MeleeTarget_t& outTarget)
{
	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	// Hoist invariant reads out of the per-target loops. The player + building
	// loops walk all entities in the group; pLocal->m_iTeamNum(), the sort
	// mode, the FOV cap, and pWeapon->m_iItemDefinitionIndex() were being
	// fetched per iteration.
	const int nLocalTeam = pLocal->m_iTeamNum();
	const int nItemDefIndex = pWeapon->m_iItemDefinitionIndex();
	const int nSortMode = CFG::Aimbot_Melee_Sort;
	const float flFOVLimit = CFG::Aimbot_Melee_FOV;

	m_vecTargets.clear();

	// Find player targets
	if (CFG::Aimbot_Target_Players)
	{
		auto group{ nItemDefIndex == Soldier_t_TheDisciplinaryAction ? EEntGroup::PLAYERS_ALL : EEntGroup::PLAYERS_ENEMIES };

		if (!CFG::Aimbot_Melee_Whip_Teammates)
		{
			group = EEntGroup::PLAYERS_ENEMIES;
		}

		for (const auto pEntity : H::Entities->GetGroup(group))
		{
			if (!pEntity)
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

				if (nItemDefIndex != Heavy_t_TheHolidayPunch && CFG::Aimbot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
					continue;

				if (CFG::Aimbot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
					continue;
			}

			if (pPlayer->m_iTeamNum() != nLocalTeam && CFG::Aimbot_Melee_Target_LagRecords)
			{
				int nRecords = 0;

				if (!F::LagRecords->HasRecords(pPlayer, &nRecords))
					continue;

				const auto& cachedState = F::LagRecords->GetCachedState(pPlayer->entindex());

				for (int n = 0; n < nRecords; n++)
				{
					const auto pRecord = F::LagRecords->GetRecord(pPlayer, n);

					if (!CLagRecords::IsRecordUsable(pRecord, cachedState))
						continue;

					Vec3 vPos = SDKUtils::GetHitboxPosFromMatrix(pPlayer, HITBOX_BODY, pRecord->BoneData.data());
					Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
					const float flFOVTo = nSortMode == 0 ? Math::CalcFov(vLocalAngles, vAngleTo) : 0.0f;
					const float flDistTo = vLocalPos.DistTo(vPos);

					if (nSortMode == 0 && flFOVTo > flFOVLimit)
						continue;

					m_vecTargets.emplace_back(MeleeTarget_t{ pPlayer, vPos, vAngleTo, flFOVTo, flDistTo, pRecord->SimulationTime, pRecord });
				}
			}

			Vec3 vPos = pPlayer->GetHitboxPos(HITBOX_BODY);
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = nSortMode == 0 ? Math::CalcFov(vLocalAngles, vAngleTo) : 0.0f;
			const float flDistTo = vLocalPos.DistTo(vPos);

			if (nSortMode == 0 && flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(MeleeTarget_t{ pPlayer, vPos, vAngleTo, flFOVTo, flDistTo, pPlayer->m_flSimulationTime() });
		}
	}

	// Find building targets
	if (CFG::Aimbot_Target_Buildings)
	{
		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::BUILDINGS_ENEMIES))
		{
			if (!pEntity)
				continue;

			const auto pBuilding = pEntity->As<C_BaseObject>();

			if (pBuilding->m_bPlacing())
				continue;

			Vec3 vPos = pBuilding->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = nSortMode == 0 ? Math::CalcFov(vLocalAngles, vAngleTo) : 0.0f;
			const float flDistTo = vLocalPos.DistTo(vPos);

			if (nSortMode == 0 && flFOVTo > flFOVLimit)
				continue;

			m_vecTargets.emplace_back(MeleeTarget_t{ pBuilding, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	if (m_vecTargets.empty())
		return false;

	// Sort by target priority
	F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Melee_Sort);

	const int itEnd = std::min(4, static_cast<int>(m_vecTargets.size()));

	// Find and return the first valid target
	for (int n = 0; n < itEnd; n++)
	{
		auto& target = m_vecTargets[n];

		if (!CanSee(pLocal, pWeapon, target))
			continue;

		outTarget = target;
		return true;
	}

	return false;
}

bool CAimbotMelee::ShouldAim(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon)
{
	return CFG::Aimbot_Melee_Aim_Type != 1 || IsFiring(pCmd, pWeapon);
}

void CAimbotMelee::Aim(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vAngles)
{
	Vec3 vAngleTo = vAngles - pLocal->m_vecPunchAngle();
	Math::ClampAngles(vAngleTo);

	switch (CFG::Aimbot_Melee_Aim_Type)
	{
		// Plaint
		case 0:
		{
			pCmd->viewangles = vAngleTo;
			break;
		}

		// Silent
		case 1:
		{
			if (IsFiring(pCmd, pWeapon))
			{
				H::AimUtils->FixMovement(pCmd, vAngleTo);
				pCmd->viewangles = vAngleTo;

				if (Shifting::bShifting && Shifting::bShiftingWarp)
					G::bSilentAngles = true;

				else G::bPSilentAngles = true;
			}

			break;
		}

		// Smooth
		case 2:
		{
			Vec3 vDelta = vAngleTo - pCmd->viewangles;
			Math::ClampAngles(vDelta);

			if (vDelta.Length() > 0.0f && CFG::Aimbot_Melee_Smoothing)
			{
				pCmd->viewangles += vDelta / CFG::Aimbot_Melee_Smoothing;
			}

			break;
		}

		default: break;
	}
}

bool CAimbotMelee::ShouldFire(const MeleeTarget_t& target)
{
	return !CFG::Aimbot_AutoShoot ? false : target.MeleeTraceHit;
}

void CAimbotMelee::HandleFire(CUserCmd* pCmd, C_TFWeaponBase* pWeapon)
{
	pCmd->buttons |= IN_ATTACK;
}

bool CAimbotMelee::IsFiring(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon)
{
	if (Shifting::bShifting && Shifting::bShiftingWarp)
	{
		return true;
	}

	if (pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
	{
		return (pCmd->buttons & IN_ATTACK) && G::bCanPrimaryAttack;
	}

	return fabsf(pWeapon->m_flSmackTime() - I::GlobalVars->curtime) < I::GlobalVars->interval_per_tick * 2.0f;
}

void CAimbotMelee::Run(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (!CFG::Aimbot_Melee_Active)
		return;

	if (CFG::Aimbot_Melee_Sort == 0)
		G::flAimbotFOV = CFG::Aimbot_Melee_FOV;

	if (Shifting::bShifting && !Shifting::bShiftingWarp)
		return;

	const bool isFiring = IsFiring(pCmd, pWeapon);

	MeleeTarget_t target = {};
	if (GetTarget(pLocal, pWeapon, target) && target.Entity)
	{
		const auto aimKeyDown = H::Input->IsDown(CFG::Aimbot_Key) || CFG::Aimbot_Melee_Always_Active;
		if (aimKeyDown || isFiring)
		{
			G::nTargetIndex = target.Entity->entindex();

			// Auto shoot
			if (aimKeyDown)
			{
				if (ShouldFire(target))
				{
					HandleFire(pCmd, pWeapon);
				}
			}

			const bool bIsFiring = IsFiring(pCmd, pWeapon);
			G::bFiring = bIsFiring;

			// Are we ready to aim?
			if (ShouldAim(pCmd, pWeapon) || bIsFiring)
			{
				if (aimKeyDown)
				{
					Aim(pCmd, pLocal, pWeapon, target.AngleTo);
				}

				if (bIsFiring && target.Entity->GetClassId() == ETFClassIds::CTFPlayer)
				{
					pCmd->tick_count = TIME_TO_TICKS(target.SimulationTime + SDKUtils::GetLerp());
				}
			}

			// Walk to target
			if (CFG::Aimbot_Melee_Walk_To_Target && (pLocal->m_fFlags() & FL_ONGROUND))
			{
				SDKUtils::WalkTo(pCmd, pLocal->m_vecOrigin(), target.Position, 1.f);
			}
		}
	}
}
