#include "AutoBackstab.h"

#include "../../CFG.h"

#include "../../LagRecords/LagRecords.h"

static bool HasActiveRazorback(C_TFPlayer* pPlayer)
{
	if (!pPlayer)
	{
		return false;
	}

	// Walk the player's move-child chain (wearables/weapons) instead of a full
	// client-entity-list scan. Razorback is always parented to its owner.
	constexpr int MAX_MOVE_CHILDREN = 64;
	int nChild = 0;

	for (C_BaseEntity* pAttach = pPlayer->FirstMoveChild();
		pAttach && nChild < MAX_MOVE_CHILDREN;
		pAttach = pAttach->NextMovePeer(), ++nChild)
	{
		if (pAttach->GetClassId() != ETFClassIds::CTFWearableRazorback)
		{
			continue;
		}

		if (pAttach->ShouldDraw())
		{
			return true;
		}
	}

	return false;
}

bool CanKnifeOneShot(C_TFPlayer* target, bool crit, bool miniCrit)
{
	if (!target || target->IsInvulnerable())
	{
		return false;
	}

	const auto pWeapon{ target->m_hActiveWeapon().Get() };

	if (!pWeapon)
	{
		return false;
	}

	int dmgMult = 1;

	if (miniCrit || target->IsMarked())
	{
		dmgMult = 2;
	}

	if (crit)
	{
		dmgMult = 3;
	}

	if (pWeapon->As<C_TFWeaponBase>()->m_iItemDefinitionIndex() == Heavy_t_FistsofSteel)
	{
		return target->m_iHealth() <= 80 * dmgMult;
	}

	return target->m_iHealth() <= 40 * dmgMult;
}

void CAutoBackstab::Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!CFG::Triggerbot_AutoBackstab_Active)
	{
		return;
	}

	if (!G::bCanPrimaryAttack || pLocal->m_bFeignDeathReady() || pLocal->m_flInvisibility() > 0.0f || pWeapon->GetWeaponID() != TF_WEAPON_KNIFE)
	{
		return;
	}

	// The melee aimbot runs before the triggerbot and may already have resolved
	// this command against a specific pose. Stamping our own pick over it - which
	// this function used to do unconditionally, for both the live and the
	// historical branch - retargets the already-resolved swing to a different
	// player or a different point in time, so neither lands.
	if (G::bCommandTickResolved)
	{
		return;
	}

	// A manually-fired knife command belongs to the post-aimbot historical
	// melee resolver. AutoBackstab must not select a different player/pose when
	// that resolver deliberately leaves the incoming tick untouched.
	if (G::bManualMeleeFiring)
	{
		return;
	}

	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	// Hoist invariant reads. pLocal->GetShootPos() was being called 3x per
	// target (FOV check, angle calc, trace) and pLocal->GetCenter() once.
	const Vec3 vShootPos = pLocal->GetShootPos();
	const Vec3 vLocalCenter = pLocal->GetCenter();
	const float flSwingRange = pWeapon->GetSwingRange(); // knife returns 48; matches the game's real melee reach

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
	{
		if (!pEntity)
		{
			continue;
		}

		const auto pPlayer = pEntity->As<C_TFPlayer>();

		if (!pPlayer || pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
		{
			continue;
		}

		const Vec3 vTargetCenter = pPlayer->GetCenter();

		if (CFG::Triggerbot_AutoBackstab_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
		{
			continue;
		}

		if (CFG::Triggerbot_AutoBackstab_Ignore_Invisible && pPlayer->IsInvisible())
		{
			continue;
		}

		if (CFG::Triggerbot_AutoBackstab_Ignore_Invulnerable && pPlayer->IsInvulnerable())
		{
			continue;
		}

		if (CFG::Triggerbot_AutoBackstab_Ignore_Razorback && HasActiveRazorback(pPlayer))
		{
			continue;
		}

		auto canKnife = false;
		if (CFG::Triggerbot_AutoBackstab_Knife_If_Lethal)
		{
			canKnife = CanKnifeOneShot(pPlayer, pLocal->IsCritBoosted(), pLocal->IsMiniCritBoosted());
		}

		bool bInFOV = true;
		if (CFG::Triggerbot_AutoBackstab_FOV > 0.0f)
		{
			const Vec3 vAngToTarget = Math::CalcAngle(vShootPos, vTargetCenter);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngToTarget);

			if (flFOVTo > CFG::Triggerbot_AutoBackstab_FOV)
			{
				bInFOV = false;
			}
		}

		if (bInFOV && (canKnife || H::AimUtils->IsBehindAndFacingTarget(
			vLocalCenter, vTargetCenter, vLocalAngles, pPlayer->GetEyeAngles())))
		{
			Vec3 forward{};
			Math::AngleVectors(vLocalAngles, &forward);

			auto to = vShootPos + (forward * flSwingRange);

			if (H::AimUtils->TraceEntityMelee(pPlayer, vShootPos, to))
			{
				pCmd->buttons |= IN_ATTACK;

				// Deliberately leave tick_count alone. This branch traced the
				// live pose, i.e. the *interpolated* present (~curtime - lerp),
				// while m_flSimulationTime is the newest server update the
				// player has. Stamping m_flSimulationTime + lerp asked the
				// server to rewind to a pose ahead of the one we swung at - and
				// to a pose that was never recorded in the first place. The
				// incoming tick already carries the server's own latency
				// correction, which is what this swing was aimed with. Claim
				// ownership so no later feature rewinds it.
				G::bCommandTickResolved = true;

				return;
			}
		}

		if (!CFG::Triggerbot_AutoBackstab_Use_LagRecords)
		{
			continue;
		}

		int numRecords = 0;

		if (!F::LagRecords->HasRecords(pPlayer, &numRecords))
		{
			continue;
		}

		const auto& cachedState = F::LagRecords->GetCachedState(pPlayer->entindex());

		for (int n = 0; n < numRecords; n++)
		{
			const auto record = F::LagRecords->GetRecord(pPlayer, n);

			if (!CLagRecords::IsRecordUsable(record, cachedState))
				continue;

			if (CFG::Triggerbot_AutoBackstab_FOV > 0.0f)
			{
				const Vec3 vAngToRecord = Math::CalcAngle(vShootPos, record->Center);
				if (Math::CalcFov(vLocalAngles, vAngToRecord) > CFG::Triggerbot_AutoBackstab_FOV)
					continue;
			}

			if (H::AimUtils->IsBehindAndFacingTarget(
				vLocalCenter, record->Center, vLocalAngles, record->EyeAngles))
			{
				{
					CLagRecordScope scope(record);

					// A record that failed to install leaves the live pose in
					// place, so the trace below would be validating the present
					// while the stamp further down commits this record's
					// historical tick - a stab that connects on screen and
					// misses on the server. Skip the record instead.
					if (!scope.IsActive())
						continue;

					Vec3 forward{};
					Math::AngleVectors(vLocalAngles, &forward);

					auto to = vShootPos + (forward * flSwingRange);

					if (!H::AimUtils->TraceEntityMelee(pPlayer, vShootPos, to))
						continue;
				}

				pCmd->buttons |= IN_ATTACK;

				// Same pose/tick pairing every other consumer uses: records store
				// the pose time, GetCommandTick re-adds the lerp the server
				// subtracts. Claim the tick so nothing downstream retargets it.
				pCmd->tick_count = CLagRecords::GetCommandTick(record->SimulationTime);
				G::bCommandTickResolved = true;

				return;
			}
		}
	}
}
