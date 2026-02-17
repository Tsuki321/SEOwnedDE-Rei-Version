#pragma once

#include "../../../../SDK/SDK.h"

class CAutoShoot
{
	float GetHitboxScale(int nHitboxGroup);
	bool IsHitboxUnderCrosshair(C_TFPlayer* pLocal, C_TFPlayer* pPlayer, int nHitbox, float flScale);

public:
	void Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd);
};

MAKE_SINGLETON_SCOPED(CAutoShoot, AutoShoot, F);
