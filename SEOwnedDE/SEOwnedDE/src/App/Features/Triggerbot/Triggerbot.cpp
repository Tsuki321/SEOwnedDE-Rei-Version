#include "Triggerbot.h"

#include "../CFG.h"

#include "AutoAirblast/AutoAirblast.h"
#include "AutoBackstab/AutoBackstab.h"
#include "AutoDetonate/AutoDetonate.h"
#include "AutoDetonateFlares/AutoDetonateFlares.h"
#include "AutoShoot/AutoShoot.h"
#include "AutoVaccinator/AutoVaccinator.h"

void CTriggerbot::Run(CUserCmd* pCmd)
{
	if (!CFG::Triggerbot_Active)
		return;

	const auto pLocal = H::Entities->GetLocal();

	if (!pLocal || pLocal->deadflag()
		|| pLocal->InCond(TF_COND_HALLOWEEN_GHOST_MODE)
		|| pLocal->InCond(TF_COND_HALLOWEEN_BOMB_HEAD)
		|| pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return;

	const auto pWeapon = H::Entities->GetWeapon();

	if (!pWeapon)
		return;

	// AutoDetonateFlares dispatches BEFORE the parent Triggerbot_Key gate so its own
	// Require_Key / Ignore_Key toggles can independently honour or bypass the master key.
	F::AutoDetonateFlares->Run(pLocal, pWeapon, pCmd);

	if (CFG::Triggerbot_Key && !H::Input->IsDown(CFG::Triggerbot_Key))
		return;

	F::AutoAirblast->Run(pLocal, pWeapon, pCmd);
	F::AutoBackstab->Run(pLocal, pWeapon, pCmd);
	F::AutoDetonate->Run(pLocal, pWeapon, pCmd);
	F::AutoShoot->Run(pLocal, pWeapon, pCmd);
	F::AutoVaccinator->Run(pLocal, pWeapon, pCmd);
}
