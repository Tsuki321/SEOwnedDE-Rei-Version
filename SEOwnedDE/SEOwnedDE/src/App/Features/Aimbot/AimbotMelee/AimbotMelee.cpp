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

		// A record that failed to install leaves the live pose in place, so the
		// traces below would be validating the present while Run() goes on to
		// stamp this record's historical tick - a swing that visibly connects on
		// screen and misses on the server. Treat it as not visible instead.
		if (target.LagRecord && !scope.IsActive())
		{
			target.MeleeTraceHit = false;
			return false;
		}

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

				// Empty ring: skip records, still offer the live pose below.
				if (F::LagRecords->HasRecords(pPlayer, &nRecords))
				{
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

	constexpr std::size_t kTargetsToTrace = 4;
	F::AimbotCommon->SortFirst(m_vecTargets, kTargetsToTrace, nSortMode);

	const int itEnd = std::min(static_cast<int>(kTargetsToTrace), static_cast<int>(m_vecTargets.size()));

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

bool CAimbotMelee::CaptureManualSwingCommand(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon)
{
	m_bManualSwingImpactCommand = false;

	if (!pCmd || !pWeapon || !I::GlobalVars || pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
		return false;

	if (m_bManualSwingPending
		&& (m_pManualSwingWeapon != pWeapon
			|| I::GlobalVars->curtime > m_flManualSwingExpireTime))
	{
		ResetManualSwingState();
	}

	const bool bHadPendingSwing = m_bManualSwingPending;
	const bool bUserStartedSwing = (pCmd->buttons & IN_ATTACK) && G::bCanPrimaryAttack;
	if (!m_bManualSwingPending && bUserStartedSwing)
	{
		m_pManualSwingWeapon = pWeapon;
		m_flManualSwingExpireTime = I::GlobalVars->curtime + 0.5f;
		m_bManualSwingPending = true;
	}

	if (!m_bManualSwingPending)
		return false;

	const float flSmackTime = pWeapon->m_flSmackTime();
	const float flImpactTolerance = I::GlobalVars->interval_per_tick * 2.0f;
	const bool bImpactDue = flSmackTime > 0.0f
		&& I::GlobalVars->curtime >= flSmackTime
		&& I::GlobalVars->curtime - flSmackTime <= flImpactTolerance;

	// The swing must have been pending before this command. This keeps a stale
	// smack timestamp from resolving a newly initiated swing, while still
	// allowing held attack to resolve the real delayed impact.
	if (bHadPendingSwing && bImpactDue)
	{
		m_bManualSwingImpactCommand = true;
	}

	// Keep ownership for the whole bounded swing window. The initiating command
	// starts the weapon's swing but does not carry delayed damage yet; the caller
	// separately checks m_bManualSwingImpactCommand before resolving history.
	return true;
}

void CAimbotMelee::FinishManualSwingCommand(bool bResolved)
{
	if (m_bManualSwingImpactCommand || bResolved)
		ResetManualSwingState();
}

void CAimbotMelee::ResetManualSwingState()
{
	m_pManualSwingWeapon = nullptr;
	m_flManualSwingExpireTime = -1.0f;
	m_bManualSwingPending = false;
	m_bManualSwingImpactCommand = false;
}

bool CAimbotMelee::ResolveManualSwing(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	if (!pCmd || !pLocal || !pWeapon)
		return false;

	const Vec3 vTraceStart = pLocal->GetShootPos();
	Vec3 vSwingAngles = pCmd->viewangles + pLocal->m_vecPunchAngle();
	Math::ClampAngles(vSwingAngles);

	Vec3 vForward = {};
	Math::AngleVectors(vSwingAngles, &vForward);
	const Vec3 vTraceEnd = vTraceStart + (vForward * pWeapon->GetSwingRange());
	const Vec3 vLocalCenter = pLocal->GetCenter();
	const bool bKnife = pWeapon->GetWeaponID() == TF_WEAPON_KNIFE;

	const LagRecord_t* pBestRecord = nullptr;
	C_TFPlayer* pBestPlayer = nullptr;

	if (CFG::Aimbot_Melee_Manual_Backtrack
		&& CFG::Aimbot_Target_Players && CFG::Aimbot_Melee_Target_LagRecords)
	{
		for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
		{
			if (!pEntity)
				continue;

			const auto pPlayer = pEntity->As<C_TFPlayer>();
			if (!pPlayer || pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
				continue;

			if (CFG::Aimbot_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
				continue;

			if (CFG::Aimbot_Ignore_Invisible && pPlayer->IsInvisible())
				continue;

			if (CFG::Aimbot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
				continue;

			if (CFG::Aimbot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
				continue;

			int nRecords = 0;
			if (!F::LagRecords->HasRecords(pPlayer, &nRecords))
				continue;

			const auto& cachedState = F::LagRecords->GetCachedState(pPlayer->entindex());
			for (int n = 0; n < nRecords; ++n)
			{
				const auto pRecord = F::LagRecords->GetRecord(pPlayer, n);
				if (!pRecord)
					continue;

				if (!CLagRecords::IsRecordUsable(pRecord, cachedState))
					continue;

				// A knife tick is only valid when this exact historical pose satisfies
				// the server's backstab-facing geometry. A hull overlap alone can be a
				// front slash and must not be stamped as a backstab record.
				if (bKnife && !H::AimUtils->IsBehindAndFacingTarget(
					vLocalCenter, pRecord->Center, vSwingAngles, pRecord->EyeAngles))
					continue;

				CLagRecordScope scope(pRecord);
				if (!scope.IsActive())
					continue;

				if (!H::AimUtils->TraceEntityMelee(pPlayer, vTraceStart, vTraceEnd))
					continue;

				pBestRecord = pRecord;
				pBestPlayer = pPlayer;
				break;
			}
		}
	}

	if (pBestRecord && pBestPlayer)
	{
		pCmd->tick_count = CLagRecords::GetCommandTick(pBestRecord->SimulationTime);
		G::bCommandTickResolved = true;
		G::nTargetIndexEarly = pBestPlayer->entindex();
		G::nTargetIndex = pBestPlayer->entindex();
		return true;
	}

	// No historical pose on the hull. Accuracy Improvements pins live bones
	// to the newest network pose, so a live hit must be paired with
	// GetCommandTick(simTime) or the server rewinds to the interpolated
	// present and the swing misses. Vanilla interpolation already matches
	// vanilla tick_count, so leave the command untouched in that mode.
	if (!CFG::Misc_Accuracy_Improvements)
		return false;

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
	{
		if (!pEntity)
			continue;

		const auto pPlayer = pEntity->As<C_TFPlayer>();
		if (!pPlayer || pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
			continue;

		if (CFG::Aimbot_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
			continue;

		if (CFG::Aimbot_Ignore_Invisible && pPlayer->IsInvisible())
			continue;

		if (CFG::Aimbot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
			continue;

		if (CFG::Aimbot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
			continue;

		if (!H::AimUtils->TraceEntityMelee(pPlayer, vTraceStart, vTraceEnd))
			continue;

		pCmd->tick_count = CLagRecords::GetCommandTick(pPlayer->m_flSimulationTime());
		G::bCommandTickResolved = true;
		G::nTargetIndexEarly = pPlayer->entindex();
		G::nTargetIndex = pPlayer->entindex();
		return true;
	}

	return false;
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
	const bool aimKeyDown = H::Input->IsDown(CFG::Aimbot_Key) || CFG::Aimbot_Melee_Always_Active;
	const bool manualFireIntent = pCmd->buttons & IN_ATTACK;
	const bool needsTargetScan = aimKeyDown || manualFireIntent || isFiring;
	if (!needsTargetScan)
		return;

	MeleeTarget_t target = {};
	if (GetTarget(pLocal, pWeapon, target) && target.Entity)
	{
		if (aimKeyDown || isFiring)
		{
			G::nTargetIndex = target.Entity->entindex();

			// Knife autoshoot is owned by AutoBackstab so a front slash cannot
			// consume the swing / claim the tick.
			if (aimKeyDown && pWeapon->GetWeaponID() != TF_WEAPON_KNIFE)
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

				// Hand-aimed swings are resolved after Run() from the finalized command
				// angle and real melee hull. Letting this center-angle branch claim them
				// first would bypass the exact contact test and, for knives, the matching
				// historical backstab-facing validation.
				if (bIsFiring && !G::bManualMeleeFiring
					&& target.Entity->GetClassId() == ETFClassIds::CTFPlayer)
				{
					// Same rule as hitscan: a record's tick only describes the
					// shot if the command is actually pointing at that record.
					// Aim() snaps for Plain/Silent, eases for Smooth, and does
					// not run at all with the aim key up (Aimbot_Key is unbound
					// by default, though Aimbot_Melee_Always_Active can stand in
					// for it), so verify the resulting angles instead of assuming.
					Vec3 vAimError = target.AngleTo - pLocal->m_vecPunchAngle() - pCmd->viewangles;
					Math::ClampAngles(vAimError);

					constexpr float flAimedEpsilon = 0.01f;
					const bool bAimbotDirectedSwing = vAimError.LengthSqr() <= flAimedEpsilon * flAimedEpsilon;

					if (bAimbotDirectedSwing)
					{
						// Historical records always stamp GetCommandTick so the
						// server rewinds to the pose we swung at.
						//
						// Live pose depends on Accuracy Improvements:
						// - On: live bones are the newest network pose;
						//   GetCommandTick(simTime) pairs the command with it
						//   (the server subtracts lerp).
						// - Off: live bones are the interpolated present;
						//   leaving vanilla tick_count is correct.
						if (target.LagRecord || CFG::Misc_Accuracy_Improvements)
							pCmd->tick_count = CLagRecords::GetCommandTick(target.SimulationTime);

						// Claim the command even when the winner was a live pose.
						// Without the flag, AutoBackstab reads "nobody owns this"
						// and rewinds a swing the melee aimbot had already aimed.
						G::bCommandTickResolved = true;
					}
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
