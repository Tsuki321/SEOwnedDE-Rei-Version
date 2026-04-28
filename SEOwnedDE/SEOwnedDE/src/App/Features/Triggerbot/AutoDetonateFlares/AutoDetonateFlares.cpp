#include "AutoDetonateFlares.h"

#include "../../CFG.h"

// Auto Detonate Flares
//
// Ported from Unibox-master's CAutoDetonate::FlareCheck
// (Unibox-master/Amalgam/src/Features/Aimbot/AutoDetonate/AutoDetonate.cpp:256).
//
// Walks all flare projectiles whose launcher is owned by the local player and, when an
// enemy player falls inside the explosion radius (with optional ping prediction), issues
// IN_ATTACK2 to detonate (Detonator/Scorch Shot remote-detonate behavior).
//
// Activation gate (per user spec):
//   - Active        -> master toggle
//   - Require_Key   -> only run while Triggerbot_Key is held (default ON)
//   - Ignore_Key    -> bypass the key gate entirely (overrides Require_Key)
// Note: Triggerbot.cpp dispatches this feature BEFORE the parent Triggerbot_Key gate so
// Ignore_Key can actually bypass it.
void CAutoDetonateFlares::Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!CFG::Triggerbot_AutoDetonateFlares_Active)
	{
		return;
	}

	// Per-feature key gate. Ignore_Key wins over Require_Key.
	if (!CFG::Triggerbot_AutoDetonateFlares_Ignore_Key
		&& CFG::Triggerbot_AutoDetonateFlares_Require_Key
		&& CFG::Triggerbot_Key
		&& !H::Input->IsDown(CFG::Triggerbot_Key))
	{
		return;
	}

	if (!pLocal || pLocal->deadflag())
	{
		return;
	}

	const float flLatency = CFG::Triggerbot_AutoDetonateFlares_Account_Ping ? SDKUtils::GetLatency() : 0.0f;
	const float flRadiusScale = static_cast<float>(CFG::Triggerbot_AutoDetonateFlares_Radius) / 100.0f;

	// Iterate all live projectiles; pick out flares whose launcher belongs to the local player.
	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PROJECTILES_ALL))
	{
		if (!pEntity)
		{
			continue;
		}

		if (pEntity->GetClassId() != ETFClassIds::CTFProjectile_Flare)
		{
			continue;
		}

		const auto pFlare = pEntity->As<C_TFProjectile_Flare>();

		if (!pFlare)
		{
			continue;
		}

		// Resolve the firing weapon (Detonator / Scorch Shot / regular Flare Gun).
		// Only local-fired flares matter for auto-detonation.
		const auto launcherEnt = pFlare->m_hLauncher().Get();

		if (!launcherEnt)
		{
			continue;
		}

		const auto pWeaponLauncher = launcherEnt->As<C_TFWeaponBase>();

		if (!pWeaponLauncher)
		{
			continue;
		}

		const auto launcherOwner = pWeaponLauncher->m_hOwnerEntity().Get();

		if (launcherOwner != pLocal)
		{
			continue;
		}

		// Base radius of 110 hu (Detonator splash) scaled by user setting and any
		// mult_explosion_radius attributes on the launcher.
		float flRadius = 110.0f * flRadiusScale;
		flRadius = SDKUtils::AttribHookValue(flRadius, "mult_explosion_radius", pWeaponLauncher);

		if (flRadius <= 0.0f)
		{
			continue;
		}

		// Predict the flare's position at the time the server will see our detonate.
		Vec3 vel{};
		pFlare->EstimateAbsVelocity(vel);
		const Vec3 vPredictedOrigin = pFlare->m_vecOrigin() + (vel * flLatency);

		// Test enemies in radius.
		for (const auto pPlayerEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
		{
			if (!pPlayerEntity)
			{
				continue;
			}

			const auto pPlayer = pPlayerEntity->As<C_TFPlayer>();

			if (!pPlayer || pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
			{
				continue;
			}

			if (CFG::Triggerbot_AutoDetonateFlares_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
			{
				continue;
			}

			if (CFG::Triggerbot_AutoDetonateFlares_Ignore_Invisible && pPlayer->IsInvisible())
			{
				continue;
			}

			if (CFG::Triggerbot_AutoDetonateFlares_Ignore_Invulnerable && pPlayer->IsInvulnerable())
			{
				continue;
			}

			if (vPredictedOrigin.DistTo(pPlayer->GetCenter()) > flRadius)
			{
				continue;
			}

			// LOS check from flare to target so we don't detonate through a wall.
			if (!H::AimUtils->TraceEntityAutoDet(pPlayer, vPredictedOrigin, pPlayer->GetCenter()))
			{
				continue;
			}

			pCmd->buttons |= IN_ATTACK2;
			return;
		}
	}
}
