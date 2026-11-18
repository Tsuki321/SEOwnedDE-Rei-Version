#pragma once

#include "../../../SDK/SDK.h"

class CEnginePrediction
{
	CMoveData m_MoveData = {};
	float m_fOldCurrentTime = 0.0f;
	float m_fOldFrameTime = 0.0f;
	int m_nOldTickCount = 0;
	int m_nTick = 0;
	CUserCmd* m_pLastCmd = nullptr;
	bool m_bActive = false;

	int GetTickbase(CUserCmd* pCmd, C_TFPlayer* pLocal);

public:
	int flags{};

	void Start(CUserCmd* pCmd);
	void End();
	void Reset();
	bool IsActive() const { return m_bActive; }
};

MAKE_SINGLETON_SCOPED(CEnginePrediction, EnginePrediction, F)
