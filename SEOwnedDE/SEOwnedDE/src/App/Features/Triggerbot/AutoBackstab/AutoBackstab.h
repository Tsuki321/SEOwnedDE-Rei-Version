#pragma once

#include "../../../../SDK/SDK.h"

class CAutoBackstab
{
public:
	void Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd);

private:
	// Consecutive commands a backstab was detected while the knife was not yet
	// server-primed. Bounds the wait for m_bReadyToBackstab so an un-primed
	// stab is never suppressed outright, only delayed.
	int m_nUnprimedDetectionTicks = 0;
};

MAKE_SINGLETON_SCOPED(CAutoBackstab, AutoBackstab, F);
