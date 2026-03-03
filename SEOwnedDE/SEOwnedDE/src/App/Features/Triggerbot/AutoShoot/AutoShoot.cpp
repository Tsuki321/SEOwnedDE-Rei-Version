#include "AutoShoot.h"

#include "../../CFG.h"

// Returns the hitbox scale factor based on the hitbox group.
// Head: strict (small scale = only fires when deep inside hitbox)
// Body/torso: lenient (full or nearly full hitbox)
// Arms/legs: medium strictness
float CAutoShoot::GetHitboxScale(int nHitboxGroup)
{
	switch (nHitboxGroup)
	{
		case HITGROUP_HEAD:
			return CFG::Triggerbot_AutoShoot_Head_Scale;

		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			return CFG::Triggerbot_AutoShoot_Body_Scale;

		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			return CFG::Triggerbot_AutoShoot_Other_Scale;

		default:
			return CFG::Triggerbot_AutoShoot_Body_Scale;
	}
}

// Checks if the crosshair ray intersects a specific hitbox, with the OBB scaled by flScale.
// A scale < 1.0 shrinks the hitbox (stricter), scale >= 1.0 keeps it normal or expands (lenient).
bool CAutoShoot::IsHitboxUnderCrosshair(C_TFPlayer* pLocal, C_TFPlayer* pPlayer, int nHitbox, float flScale)
{
	Vec3 vCenter = {}, vMins = {}, vMaxs = {};
	matrix3x4_t matrix = {};
	pPlayer->GetHitboxInfo(nHitbox, &vCenter, &vMins, &vMaxs, &matrix);

	// Scale the OBB bounds to control strictness
	vMins *= flScale;
	vMaxs *= flScale;

	Vec3 vForward = {};
	Math::AngleVectors(I::EngineClient->GetViewAngles(), &vForward);
	const Vec3 vTraceStart = pLocal->GetShootPos();

	return Math::RayToOBB(vTraceStart, vForward, vCenter, vMins, vMaxs, matrix);
}

void CAutoShoot::Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!CFG::Triggerbot_AutoShoot_Active)
		return;

	// Only work with hitscan weapons
	if (H::AimUtils->GetWeaponType(pWeapon) != EWeaponType::HITSCAN)
		return;

	// Don't fire if we can't attack yet
	if (!G::bCanPrimaryAttack)
		return;

	// Don't fire if weapon has no ammo
	if (!pWeapon->HasPrimaryAmmoForShot())
		return;

	// Wait for headshot capability if configured
	if (CFG::Triggerbot_AutoShoot_Wait_For_Headshot && H::AimUtils->IsWeaponCapableOfHeadshot(pWeapon) && !G::bCanHeadshot)
		return;

	// Don't interfere if aimbot is already firing
	if (G::bFiring)
		return;

	const Vec3 vLocalPos = pLocal->GetShootPos();
	Vec3 vForward = {};
	Math::AngleVectors(I::EngineClient->GetViewAngles(), &vForward);
	const Vec3 vTraceEnd = vLocalPos + (vForward * 8192.0f);

	// Iterate over enemy players
	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ENEMIES))
	{
		if (!pEntity)
			continue;

		const auto pPlayer = pEntity->As<C_TFPlayer>();

		if (!pPlayer || pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
			continue;

		if (CFG::Triggerbot_AutoShoot_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
			continue;

		if (CFG::Triggerbot_AutoShoot_Ignore_Invisible && pPlayer->IsInvisible())
			continue;

		if (CFG::Triggerbot_AutoShoot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
			continue;

		if (CFG::Triggerbot_AutoShoot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
			continue;

		// First, do a basic bullet trace to see if our crosshair ray hits this player at all
		int nHitHitbox = -1;
		if (!H::AimUtils->TraceEntityBullet(pPlayer, vLocalPos, vTraceEnd, &nHitHitbox))
			continue;

		if (nHitHitbox < 0)
			continue;

		// Get the hitbox group to determine strictness
		const int nHitboxGroup = pPlayer->GetHitboxGroup(nHitHitbox);
		if (nHitboxGroup < 0)
			continue;

		// Get the appropriate scale for this hitbox group
		const float flScale = GetHitboxScale(nHitboxGroup);

		// Now do the scaled OBB check to ensure we're sufficiently inside the hitbox
		if (!IsHitboxUnderCrosshair(pLocal, pPlayer, nHitHitbox, flScale))
			continue;

		// All checks passed - fire!
		pCmd->buttons |= IN_ATTACK;

		if (CFG::Misc_Accuracy_Improvements)
		{
			pCmd->tick_count = TIME_TO_TICKS(pPlayer->m_flSimulationTime() + SDKUtils::GetLerp());
		}

		return;
	}
}
