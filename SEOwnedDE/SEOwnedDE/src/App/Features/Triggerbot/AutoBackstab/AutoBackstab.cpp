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

bool IsBehindAndFacingTarget(const Vec3& ownerCenter, const Vec3& ownerViewangles, const Vec3& targetCenter, const Vec3& targetEyeAngles)
{
	Vec3 toTarget = targetCenter - ownerCenter;
	toTarget.z = 0.0f;
	toTarget.NormalizeInPlace();

	Vec3 ownerForward{};
	Math::AngleVectors(ownerViewangles, &ownerForward, nullptr, nullptr);
	ownerForward.z = 0.0f;
	ownerForward.NormalizeInPlace();

	Vec3 targetForward{};
	Math::AngleVectors(targetEyeAngles, &targetForward, nullptr, nullptr);
	targetForward.z = 0.0f;
	targetForward.NormalizeInPlace();

	return toTarget.Dot(targetForward) > (0.0f + 0.03125f)
		&& toTarget.Dot(ownerForward) > (0.5f + 0.03125f)
		&& targetForward.Dot(ownerForward) > (-0.3f + 0.03125f);
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

static bool ApplyAimMode(CUserCmd* pCmd, const Vec3& angleTo)
{
	switch (CFG::Triggerbot_AutoBackstab_Aim_Mode)
	{
		case 0:
		{
			pCmd->viewangles = angleTo;
			return true;
		}

		case 1:
		{
			pCmd->viewangles = angleTo;
			G::bPSilentAngles = true;
			return true;
		}

		case 2:
		{
			Vec3 vDelta = angleTo - pCmd->viewangles;
			Math::ClampAngles(vDelta);

			if (vDelta.Length() > 0.0f)
			{
				pCmd->viewangles += vDelta / 6.0f;
				Math::ClampAngles(pCmd->viewangles);
			}

			Vec3 vRemaining = angleTo - pCmd->viewangles;
			Math::ClampAngles(vRemaining);
			return vRemaining.Length() <= 1.0f;
		}

		default: break;
	}

	return true;
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

	const bool bLegitMode = CFG::Triggerbot_AutoBackstab_Mode == 0;
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	// Hoist invariant reads. pLocal->GetShootPos() was being called 3x per
	// target (FOV check, angle calc, trace) and pLocal->GetCenter() once.
	const Vec3 vShootPos = pLocal->GetShootPos();
	const Vec3 vLocalCenter = pLocal->GetCenter();

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

		// GetCenter is a vfunc too; hoist the per-target read once.
		const Vec3 vTargetCenter = pPlayer->GetCenter();

		auto canKnife = false;
		if (CFG::Triggerbot_AutoBackstab_Knife_If_Lethal)
		{
			canKnife = CanKnifeOneShot(pPlayer, pLocal->IsCritBoosted(), pLocal->IsMiniCritBoosted());
		}

		if (bLegitMode && CFG::Triggerbot_AutoBackstab_FOV > 0.0f)
		{
			const Vec3 vAngToTarget = Math::CalcAngle(vShootPos, vTargetCenter);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngToTarget);

			if (flFOVTo > CFG::Triggerbot_AutoBackstab_FOV)
			{
				continue;
			}
		}

		auto angleTo{ vLocalAngles };

		if (!bLegitMode)
		{
			angleTo = Math::CalcAngle(vShootPos, vTargetCenter);
		}

		if (canKnife || IsBehindAndFacingTarget(vLocalCenter, angleTo, vTargetCenter, pPlayer->GetEyeAngles()))
		{
			Vec3 forward{};
			Math::AngleVectors(angleTo, &forward);

			auto to = vShootPos + (forward * 47.0f);

			if (H::AimUtils->TraceEntityMelee(pPlayer, vShootPos, to))
			{
				bool bReadyToAttack = true;
				if (!bLegitMode)
				{
					bReadyToAttack = ApplyAimMode(pCmd, angleTo);
				}

				if (!bReadyToAttack)
					return;

				pCmd->buttons |= IN_ATTACK;

				pCmd->tick_count = TIME_TO_TICKS(pPlayer->m_flSimulationTime() + SDKUtils::GetLerp());

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

			if (!bLegitMode)
			{
				angleTo = Math::CalcAngle(vShootPos, record->Center);
			}

			if (canKnife || IsBehindAndFacingTarget(vLocalCenter, angleTo, record->Center, record->EyeAngles))
			{
				{
					CLagRecordScope scope(record);

					Vec3 forward{};
					Math::AngleVectors(angleTo, &forward);

					auto to = vShootPos + (forward * 47.0f);

					if (!H::AimUtils->TraceEntityMelee(pPlayer, vShootPos, to))
						continue;
				}

				bool bReadyToAttack = true;
				if (!bLegitMode)
				{
					bReadyToAttack = ApplyAimMode(pCmd, angleTo);
				}

				if (!bReadyToAttack)
					return;

				pCmd->buttons |= IN_ATTACK;

				pCmd->tick_count = TIME_TO_TICKS(record->SimulationTime + SDKUtils::GetLerp());

				return;
			}
		}
	}
}
