#pragma once

#include "../../../SDK/SDK.h"

class CReadPacketState
{
	float m_flFrameTimeClientState = 0.0f;
	float m_flFrameTime = 0.0f;
	float m_flCurTime = 0.0f;
	int m_nTickCount = 0;

public:
	void Store();
	void Restore();
};

class CReadPacketGate
{
	int m_nFrame = -1;
	const void* m_pChannel = nullptr;
	bool m_bArmed = false;

public:
	bool IsArmedFor(int nFrame, const void* pChannel) const
	{
		return m_bArmed && pChannel && m_nFrame == nFrame && m_pChannel == pChannel;
	}

	void Arm(int nFrame, const void* pChannel)
	{
		m_nFrame = nFrame;
		m_pChannel = pChannel;
		m_bArmed = pChannel != nullptr;
	}

	bool Consume(int nFrame, const void* pChannel)
	{
		const bool bMatches = IsArmedFor(nFrame, pChannel);
		Reset();
		return bMatches;
	}

	void Reset()
	{
		m_nFrame = -1;
		m_pChannel = nullptr;
		m_bArmed = false;
	}
};

class CNetworkFix
{
	CReadPacketState m_State = {};
	CReadPacketGate m_ReadGate = {};

public:
	static int GetShiftCommandCapacity(int nChokedCommands)
	{
		return std::clamp(MAX_COMMANDS - std::max(0, nChokedCommands), 0, MAX_COMMANDS);
	}

	void FixInputDelay(bool bFinalTick);
	bool ShouldReadPackets();
	void Reset();
};

MAKE_SINGLETON_SCOPED(CNetworkFix, NetworkFix, F);
