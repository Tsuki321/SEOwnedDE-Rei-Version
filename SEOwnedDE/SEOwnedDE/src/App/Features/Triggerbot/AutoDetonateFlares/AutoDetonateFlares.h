#pragma once

#include "../../../../SDK/SDK.h"

class CAutoDetonateFlares
{
public:
	void Run(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, CUserCmd* pCmd);
};

MAKE_SINGLETON_SCOPED(CAutoDetonateFlares, AutoDetonateFlares, F);
