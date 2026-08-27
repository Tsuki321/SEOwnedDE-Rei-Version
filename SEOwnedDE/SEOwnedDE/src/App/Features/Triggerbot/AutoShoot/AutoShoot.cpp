#include "AutoShoot.h"

#include "../../CFG.h"

#include "../../LagRecords/LagRecords.h"

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
bool CAutoShoot::IsHitboxUnderCrosshair(C_TFPlayer* pPlayer, int nHitbox, float flScale, const Vec3& vTraceStart, const Vec3& vForward)
{
	Vec3 vCenter = {}, vMins = {}, vMaxs = {};
	matrix3x4_t matrix = {};
	pPlayer->GetHitboxInfo(nHitbox, &vCenter, &vMins, &vMaxs, &matrix);

	// Scale the OBB bounds to control strictness
	vMins *= flScale;
	vMaxs *= flScale;

	return Math::RayToOBB(vTraceStart, vForward, vCenter, vMins, vMaxs, matrix);
}

void CAutoShoot::Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!CFG::Triggerbot_AutoShoot_Active)
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
	if (G::bFiring)
		return;

	const Vec3 vLocalPos = pLocal->GetShootPos();
	Vec3 vForward = {};
	Math::AngleVectors(I::EngineClient->GetViewAngles(), &vForward);
	const Vec3 vTraceEnd = vLocalPos + (vForward * 8192.0f);

	trace_t trace = {};
	trace.hitbox = -1;
	CTraceFilterHitscan filter = {};
	H::AimUtils->Trace(vLocalPos, vTraceEnd, MASK_SHOT | CONTENTS_GRATE, &filter, &trace);

	if (!trace.m_pEnt || trace.allsolid || trace.hitbox < 0 || trace.m_pEnt->GetClassId() != ETFClassIds::CTFPlayer)
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

	const int nHitboxGroup = pPlayer->GetHitboxGroup(nHitHitbox);

	if (nHitboxGroup < 0)
		return;

	const float flScale = GetHitboxScale(nHitboxGroup);

	if (flScale < 1.0f && !IsHitboxUnderCrosshair(pPlayer, nHitHitbox, flScale, vLocalPos, vForward))
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

	// Live pose depends on Accuracy Improvements:
	// - On: the trace ran against the newest network pose, so stamp
	//   GetCommandTick(simTime). The server subtracts lerp and lands on
	//   that pose.
	// - Off: the trace ran against the interpolated present; leaving
	//   vanilla tick_count is correct.
	// Claim the command so nothing downstream retargets it either.
	if (CFG::Misc_Accuracy_Improvements)
		pCmd->tick_count = CLagRecords::GetCommandTick(pPlayer->m_flSimulationTime());
	G::bCommandTickResolved = true;
}
