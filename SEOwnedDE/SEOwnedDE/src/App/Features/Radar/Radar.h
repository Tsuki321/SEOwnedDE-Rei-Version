#pragma once

#include "../../../SDK/SDK.h"

class CRadar
{
	C_TFPlayer* m_pCachedLocal = nullptr;
	Vec3 m_vCachedLocalCenter = {};
	int m_nCachedRadarX = 0;
	int m_nCachedRadarY = 0;
	int m_nCachedRadarSize = 0;
	int m_nCachedRadarStyle = 0;
	float m_flCachedRadius = 1.0f;
	float m_flCachedCos = 1.0f;
	float m_flCachedSin = 0.0f;

	void UpdateRadarCache(C_TFPlayer* pLocal);
	void Drag();
	bool GetDrawPosition(int& x, int& y, const Vec3& vWorld);

public:
	void Run();
};

MAKE_SINGLETON_SCOPED(CRadar, Radar, F);
