#include "Aimbot.h"

#include "AimbotHitscan/AimbotHitscan.h"
#include "AimbotProjectile/AimbotProjectile.h"
#include "AimbotMelee/AimbotMelee.h"

#include "../CFG.h"

void CAimbot::RunMain(CUserCmd* pCmd)
{
	G::nTargetIndex = -1;
	G::flAimbotFOV = 0.0f;
	G::nTargetIndexEarly = -1;

	if (!CFG::Aimbot_Active || I::EngineVGui->IsGameUIVisible() || I::MatSystemSurface->IsCursorVisible() || SDKUtils::BInEndOfMatch())
		return;

	if (Shifting::bRecharging)
		return;

	const auto pLocal = H::Entities->GetLocal();
	const auto pWeapon = H::Entities->GetWeapon();

	if (!pLocal || !pWeapon
		|| pLocal->deadflag()
		|| pLocal->InCond(TF_COND_TAUNTING) || pLocal->InCond(TF_COND_PHASE)
		|| pLocal->InCond(TF_COND_HALLOWEEN_GHOST_MODE)
		|| pLocal->InCond(TF_COND_HALLOWEEN_BOMB_HEAD)
		|| pLocal->InCond(TF_COND_HALLOWEEN_KART)
		|| pLocal->m_bFeignDeathReady() || pLocal->m_flInvisibility() > 0.0f
		|| pWeapon->m_iItemDefinitionIndex() == Soldier_m_RocketJumper || pWeapon->m_iItemDefinitionIndex() == Demoman_s_StickyJumper)
		return;

	switch (H::AimUtils->GetWeaponType(pWeapon))
	{
		case EWeaponType::HITSCAN:
		{
			F::AimbotHitscan->Run(pCmd, pLocal, pWeapon);
			break;
		}

		case EWeaponType::PROJECTILE:
		{
			F::AimbotProjectile->Run(pCmd, pLocal, pWeapon);
			break;
		}

		case EWeaponType::MELEE:
		{
			F::AimbotMelee->Run(pCmd, pLocal, pWeapon);
			break;
		}

		default: break;
	}
}

void CAimbot::Run(CUserCmd* pCmd)
{
	const auto pLocal = H::Entities->GetLocal();
	const auto pWeapon = H::Entities->GetWeapon();
	F::AimbotProjectile->RunChargeLifecycle(pCmd, pLocal, pWeapon);
	bool bManualMeleeResolved = false;

	// Capture manual ownership before RunMain can add IN_ATTACK itself. The
	// triggerbot runs after the aimbot and must not replace this command's
	// historical tick (or its unchanged fallback tick).
	if (pLocal && pWeapon && !pLocal->deadflag())
	{
			switch (H::AimUtils->GetWeaponType(pWeapon))
		{
			case EWeaponType::HITSCAN:
				F::AimbotMelee->ResetManualSwingState();
				G::bManualHitscanFiring = F::AimbotHitscan->IsFiring(pCmd, pWeapon);
				break;

			case EWeaponType::MELEE:
				// Knife swings resolve on their initiating command. Other melee weapons
				// report the actual smack later, so retain only a user-started swing
				// through the bounded CAimbotMelee state machine; an aimbot-generated
				// delayed smack never enters that state.
				if (pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
				{
					F::AimbotMelee->ResetManualSwingState();
					G::bManualMeleeFiring = (pCmd->buttons & IN_ATTACK) && G::bCanPrimaryAttack;
				}
				else
				{
					G::bManualMeleeFiring = F::AimbotMelee->CaptureManualSwingCommand(pCmd, pWeapon);
				}
				break;

			default:
				F::AimbotMelee->ResetManualSwingState();
				break;
		}
	}
	else
	{
		F::AimbotMelee->ResetManualSwingState();
	}

	RunMain(pCmd);

	// Hand-aimed backtracking, deliberately resolved OUT here rather than inside
	// CAimbotHitscan::Run.
	//
	// ResolveManualShot used to be reachable only from within that Run, which
	// hangs off RunMain - so every early return above it silently dropped the
	// user's backtrack: Aimbot_Active off, the cheat menu open (cursor visible),
	// end of match, Shifting::bRecharging, taunting, phased, any cloak or a primed
	// dead ringer, a Rocket/Sticky Jumper equipped, Shifting::bShifting, the Auto
	// Scope branch, the fire-delay windows, a building or a stuck sticky winning
	// the FOV sort ahead of the player actually being shot at, and the minigun
	// spin-up hack that clears G::bCanPrimaryAttack. Nothing has returned yet at
	// this point, so the click gets its correct tick in all of those states.
	//
	// G::bManualHitscanFiring was latched BEFORE RunMain, and that is what makes
	// the minigun case work: the spin-up hack clears G::bCanPrimaryAttack during
	// RunMain, so re-deriving "am I firing" here would read false.
	//
	// The ownership flag stops this from second-guessing a decision the aimbot
	// already made - including its deliberate choice to leave tick_count alone for
	// a shot that was aimed at a live pose.
	if (G::bManualHitscanFiring && !G::bCommandTickResolved)
	{
		if (const auto pLocalManual = H::Entities->GetLocal(); pLocalManual && !pLocalManual->deadflag())
			F::AimbotHitscan->ResolveManualShot(pCmd, pLocalManual);
	}

	// Melee needs the same final-command resolution as hitscan, but validates
	// the game's real 18-unit swing hull instead of a center-angle epsilon. This
	// catches hand-aimed edge contacts and stamps the exact record that the final
	// command intersects. Manual ownership is latched before RunMain, so an
	// aimbot-generated IN_ATTACK cannot enter this path.
	if (G::bManualMeleeFiring && !G::bCommandTickResolved)
	{
		const auto pLocalManual = H::Entities->GetLocal();
		const auto pWeaponManual = H::Entities->GetWeapon();

		if (pLocalManual && pWeaponManual && !pLocalManual->deadflag()
			&& H::AimUtils->GetWeaponType(pWeaponManual) == EWeaponType::MELEE)
		{
			bManualMeleeResolved = F::AimbotMelee->ResolveManualSwing(pCmd, pLocalManual, pWeaponManual);
		}
	}

	if (pWeapon && H::AimUtils->GetWeaponType(pWeapon) == EWeaponType::MELEE)
		F::AimbotMelee->FinishManualSwingCommand(bManualMeleeResolved);

	//same-ish code below to see if we are firing manually

	// Re-fetch entities after RunMain in case they were invalidated
	const auto pLocalAfter = H::Entities->GetLocal();
	const auto pWeaponAfter = H::Entities->GetWeapon();

	if (!pLocalAfter || !pWeaponAfter
		|| pLocalAfter->deadflag()
		|| pLocalAfter->InCond(TF_COND_TAUNTING) || pLocalAfter->InCond(TF_COND_PHASE)
		|| pLocalAfter->m_bFeignDeathReady() || pLocalAfter->m_flInvisibility() > 0.0f)
		return;

	const auto nWeaponType = H::AimUtils->GetWeaponType(pWeaponAfter);

	if (!G::bFiring)
	{
		switch (nWeaponType)
		{
			case EWeaponType::HITSCAN:
			{
				G::bFiring = F::AimbotHitscan->IsFiring(pCmd, pWeaponAfter);
				break;
			}

			case EWeaponType::PROJECTILE:
			{
				G::bFiring = F::AimbotProjectile->IsFiring(pCmd, pLocalAfter, pWeaponAfter);
				break;
			}

			case EWeaponType::MELEE:
			{
				G::bFiring = F::AimbotMelee->IsFiring(pCmd, pWeaponAfter);
				break;
			}

			default: break;
		}
	}

	// Projectile NoSpread
	if (G::bFiring && nWeaponType == EWeaponType::PROJECTILE && CFG::Aimbot_Projectile_NoSpread)
	{
		SDKUtils::SharedRandomInt("SelectWeightedSequence", 0, 0, pCmd->random_seed);

		for (int i = 0; i < 6; ++i)
		{
			I::UniformRandomStream->RandomFloat();
		}

		switch (pWeaponAfter->GetWeaponID())
		{
			case TF_WEAPON_GRENADELAUNCHER:
			case TF_WEAPON_PIPEBOMBLAUNCHER:
			case TF_WEAPON_CANNON:
			{
				pCmd->command_number = SDKUtils::FindCmdNumWithSeed(pCmd->command_number, 39);
				break;
			}

			case TF_WEAPON_SYRINGEGUN_MEDIC:
			{
				pCmd->viewangles.x -= I::UniformRandomStream->RandomFloat(-1.5f, 1.5f);
				pCmd->viewangles.y -= I::UniformRandomStream->RandomFloat(-1.5f, 1.5f);
				Math::ClampAngles(pCmd->viewangles);
				G::bPSilentAngles = true;
				break;
			}

			case TF_WEAPON_COMPOUND_BOW:
			{
				Vec3 vSpread = {}, vSrc = {};
				pWeaponAfter->GetProjectileFireSetup(pLocalAfter, { 0.0f, 0.0f, 0.0f }, &vSrc, &vSpread, false, 2000.0f);
				pCmd->viewangles -= (vSpread - I::EngineClient->GetViewAngles());
				Math::ClampAngles(pCmd->viewangles);
				G::bPSilentAngles = true;
				break;
			}

			default:
			{
				if (pWeaponAfter->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka)
				{
					Vec3 vSpread = {};
					pWeaponAfter->GetSpreadAngles(vSpread);
					pCmd->viewangles -= (vSpread - I::EngineClient->GetViewAngles());
					Math::ClampAngles(pCmd->viewangles);
					G::bPSilentAngles = true;
				}

				break;
			}
		}
	}
}
