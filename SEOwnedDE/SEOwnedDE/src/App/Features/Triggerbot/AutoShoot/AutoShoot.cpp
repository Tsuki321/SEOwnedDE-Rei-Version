#include "AutoShoot.h"

#include "../../CFG.h"

#include "../../LagRecords/LagRecords.h"

// Scaling narrows the native hitbox selected by the engine trace. It must
// never turn a nearby collision hull or another hitbox into a valid shot.
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

bool CAutoShoot::IsHitboxUnderCrosshair(C_TFPlayer* pPlayer, int nHitbox, float flScale,
	const Vec3& vTraceStart, const Vec3& vForward, float flRange)
{
	Vec3 vMins = {}, vMaxs = {};
	matrix3x4_t matrix = {};
	if (!pPlayer->GetHitboxInfo(nHitbox, nullptr, &vMins, &vMaxs, &matrix))
		return false;

	return Math::RayToHitbox(vTraceStart, vForward, vMins, vMaxs, matrix, flScale, flRange);
}

void CAutoShoot::Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pWeapon || !pCmd || !CFG::Triggerbot_AutoShoot_Active)
		return;

	// Only work with hitscan weapons
	if (H::AimUtils->GetWeaponType(pWeapon) != EWeaponType::HITSCAN)
		return;

	// A manual shot owns its command tick. Hitscan has either resolved it to a
	// historical pose, stamped the live network pose when Accuracy Improvements
	// is on, or intentionally left the incoming tick unchanged.
	if (G::bManualHitscanFiring)
		return;

	// Somebody upstream already decided this command's tick. Same rule
	// AutoBackstab follows: never retarget a shot another feature has resolved.
	if (G::bCommandTickResolved)
		return;

	// An aimbot declined to act on this command because its fire delay is still
	// running. Shooting here anyway would silently defeat that delay, which is
	// worse than having no delay at all - the user believes the gate is active.
	if (G::bAimbotFireDelayed)
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

	// A shot is already resolved for this command - don't stack a second one onto
	// it. Pairing this with G::nTargetIndex would only dedup when an aimbot claimed
	// a target, and the -1 no-target sentinel makes that form of the guard fail open.
	if (G::bFiring || (pCmd->buttons & IN_ATTACK))
		return;

	float flRange = 8192.0f;
	if (const auto pWeaponInfo = pWeapon->GetWeaponInfo())
	{
		const float flWeaponRange = pWeaponInfo->GetWeaponData(TF_WEAPON_PRIMARY_MODE).m_flRange;
		if (std::isfinite(flWeaponRange) && flWeaponRange > 0.0f)
			flRange = std::min(flRange, flWeaponRange);
	}

	const Vec3 vLocalPos = pLocal->GetShootPos();
	// Aim, prediction, and recoil can change this command after the engine's
	// visible angles were sampled. Test the direction that the shot will use.
	const Vec3 vShotAngles = pCmd->viewangles + pLocal->m_vecPunchAngle();
	for (int nAxis = 0; nAxis < 3; ++nAxis)
		if (!std::isfinite(vLocalPos[nAxis]) || !std::isfinite(vShotAngles[nAxis]))
			return;
	Vec3 vForward = {};
	Math::AngleVectors(vShotAngles, &vForward);
	const Vec3 vTraceEnd = vLocalPos + (vForward * flRange);

	trace_t trace = {};
	trace.hitbox = -1;
	CTraceFilterHitscan filter = {};
	H::AimUtils->Trace(vLocalPos, vTraceEnd, MASK_SHOT | CONTENTS_GRATE, &filter, &trace);

	if (!trace.m_pEnt || trace.allsolid || trace.startsolid || trace.fraction >= 1.0f
		|| trace.hitbox < 0 || trace.m_pEnt->GetClassId() != ETFClassIds::CTFPlayer)
		return;

	const auto pPlayer = trace.m_pEnt->As<C_TFPlayer>();
	int nTargetTeam = 0;
	if (!pPlayer || !pPlayer->IsInValidTeam(&nTargetTeam) || nTargetTeam == pLocal->m_iTeamNum()
		|| pPlayer->IsDormant() || pPlayer->deadflag() || pPlayer->InCond(TF_COND_HALLOWEEN_GHOST_MODE))
		return;

	if (CFG::Triggerbot_AutoShoot_Ignore_Friends && pPlayer->IsPlayerOnSteamFriendsList())
		return;

	if (CFG::Triggerbot_AutoShoot_Ignore_Invisible && pPlayer->IsInvisible())
		return;

	if (CFG::Triggerbot_AutoShoot_Ignore_Invulnerable && pPlayer->IsInvulnerable())
		return;

	if (CFG::Triggerbot_AutoShoot_Ignore_Taunting && pPlayer->InCond(TF_COND_TAUNTING))
		return;

	const int nHitHitbox = trace.hitbox;

	// Use the group attached to the engine's hit, instead of assuming every
	// player model has the same hitbox ordering.
	const int nHitboxGroup = trace.hitgroup;

	const float flScale = GetHitboxScale(nHitboxGroup);

	if (!IsHitboxUnderCrosshair(pPlayer, nHitHitbox, flScale, vLocalPos, vForward, flRange))
		return;

	if (CFG::Triggerbot_AutoShoot_Wait_For_Headshot && H::AimUtils->IsWeaponCapableOfHeadshot(pWeapon)
		&& nHitboxGroup != HITGROUP_HEAD)
		return;

	if (pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC)
	{
		if (G::nOldButtons & IN_ATTACK)
			pCmd->buttons &= ~IN_ATTACK;
		else
			pCmd->buttons |= IN_ATTACK;
	}
	else
	{
		pCmd->buttons |= IN_ATTACK;
	}

	G::bFiring = true;

	// Historical shots use the stored pose time; live Accuracy shots
	// use the last rendered pose, even after local prediction advances.
	if (CFG::Misc_Accuracy_Improvements)
		pCmd->tick_count = F::LagRecords->GetLiveCommandTick(pCmd->tick_count);
	G::bCommandTickResolved = true;
}
