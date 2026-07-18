#pragma once

#include "../../../../SDK/SDK.h"

class CAutoShoot
{
	float GetHitboxScale(int nHitboxGroup);
	bool IsHitboxUnderCrosshair(C_TFPlayer* pPlayer, int nHitbox, float flScale, const Vec3& vTraceStart, const Vec3& vForward);

public:
	void Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd);
};

MAKE_SINGLETON_SCOPED(CAutoShoot, AutoShoot, F);
