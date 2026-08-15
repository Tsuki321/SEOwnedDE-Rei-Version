#include "AutoDetonate.h"

#include "../../CFG.h"

void CAutoDetonate::Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!CFG::Triggerbot_AutoDetonate_Active)
		return;

	if (!pLocal)
		return;

	if (pLocal->m_iClass() != TF_CLASS_DEMOMAN)
		return;

	// pLocal->GetShootPos() is used by the defensive-bomb CalcAngle. Hoist
	// so it isn't fetched multiple times when multiple stickies hit.
	const Vec3 vShootPos = pLocal->GetShootPos();

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PROJECTILES_LOCAL_STICKIES))
	{
		if (!pEntity)
			continue;

		const auto pSticky = pEntity->As<C_TFGrenadePipebombProjectile>();

		if (!pSticky || pSticky->m_iType() == TF_GL_MODE_REMOTE_DETONATE_PRACTICE)
			continue;

		if (I::GlobalVars->curtime < pSticky->m_flCreationTime() + SDKUtils::AttribHookValue(0.8f, "sticky_arm_time", pLocal))
			continue;

		const float flRadius = pSticky->m_bTouched() ? 150.0f : 100.0f;

		// Hoist pSticky->GetCenter() out of the inner player+building loops. The
		// vfunc was being called 4 times per sticky (2 in player loop, 2 in
		// building loop). Also re-reads on line 56/83 of the defensive
		// branch - those are single-shot after the per-target trace succeeds,
		// so the hoisted vStickyCenter is fine there too.
		const Vec3 vStickyCenter = pSticky->GetCenter();

		// Auto detonate players
		if (CFG::Triggerbot_AutoDetonate_Target_Players)
		{
			for (const auto pPlayerEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
			{
				if (!pPlayerEntity)
					continue;

				const auto pPlayer = pPlayerEntity->As<C_TFPlayer>();

				if (pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
					continue;

				if (CFG::Triggerbot_AutoDetonate_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
					continue;

				if (CFG::Triggerbot_AutoDetonate_Ignore_Invisible && pPlayer->IsInvisible())
					continue;

				if (CFG::Triggerbot_AutoDetonate_Ignore_Invulnerable && pPlayer->IsInvulnerable())
					continue;

				const Vec3 vPlayerCenter = pPlayer->GetCenter();

				if (vStickyCenter.DistTo(vPlayerCenter) < flRadius)
				{
					if (H::AimUtils->TraceEntityAutoDet(pPlayer, vStickyCenter, vPlayerCenter))
					{
						if (pSticky->m_bDefensiveBomb())
						{
							const Vec3 vOriginalAngles = pCmd->viewangles;
							const Vec3 vAngle = Math::CalcAngle(vShootPos, vStickyCenter);
							H::AimUtils->FixMovement(pCmd, vOriginalAngles);
							pCmd->viewangles = vAngle;
							G::bSilentAngles = true;
						}

						pCmd->buttons |= IN_ATTACK2;
						return;
					}
				}
			}
		}

		// Auto detonate buildings
		if (CFG::Triggerbot_AutoDetonate_Target_Buildings)
		{
			for (const auto pBuildingEntity : H::Entities->GetGroup(EEntGroup::BUILDINGS_ENEMIES))
			{
				if (!pBuildingEntity)
					continue;

				const Vec3 vBuildingCenter = pBuildingEntity->GetCenter();

				if (vStickyCenter.DistTo(vBuildingCenter) < flRadius)
				{
					if (H::AimUtils->TraceEntityAutoDet(pBuildingEntity, vStickyCenter, vBuildingCenter))
					{
						if (pSticky->m_bDefensiveBomb())
						{
							const Vec3 vOriginalAngles = pCmd->viewangles;
							const Vec3 vAngle = Math::CalcAngle(vShootPos, vStickyCenter);
							H::AimUtils->FixMovement(pCmd, vOriginalAngles);
							pCmd->viewangles = vAngle;
							G::bSilentAngles = true;
						}

						pCmd->buttons |= IN_ATTACK2;
						return;
					}
				}
			}
		}
	}
}
