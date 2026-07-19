#include "NetworkFix.h"

#include "../CFG.h"

MAKE_SIGNATURE(CL_ReadPackets, "engine.dll", "4C 8B DC 49 89 5B ? 55 56 57 41 54 41 55 41 56 41 57 48 83 EC ? 48 8B 05", 0x0);

MAKE_HOOK(CL_ReadPackets, Signatures::CL_ReadPackets.Get(), void, __cdecl,
	bool bFinalTick)
{
	if (!CFG::Misc_Ping_Reducer)
	{
		F::NetworkFix->Reset();
		CALL_ORIGINAL(bFinalTick);

		return;
	}

	if (F::NetworkFix->ShouldReadPackets())
	{
		CALL_ORIGINAL(bFinalTick);
	}
}

void CReadPacketState::Store()
{
	m_flFrameTimeClientState = I::ClientState->m_frameTime;
	m_flFrameTime = I::GlobalVars->frametime;
	m_flCurTime = I::GlobalVars->curtime;
	m_nTickCount = I::GlobalVars->tickcount;
}

void CReadPacketState::Restore()
{
	I::ClientState->m_frameTime = m_flFrameTimeClientState;
	I::GlobalVars->frametime = m_flFrameTime;
	I::GlobalVars->curtime = m_flCurTime;
	I::GlobalVars->tickcount = m_nTickCount;
}

void CNetworkFix::FixInputDelay(bool bFinalTick)
{
	if (!CFG::Misc_Ping_Reducer || !I::EngineClient || !I::ClientState || !I::GlobalVars || !I::EngineClient->IsInGame())
	{
		Reset();
		return;
	}

	const auto pNetChannel = I::EngineClient->GetNetChannelInfo();
	if (!pNetChannel || pNetChannel->IsLoopback())
	{
		Reset();
		return;
	}

	const int nFrame = I::GlobalVars->framecount;
	if (m_ReadGate.IsArmedFor(nFrame, pNetChannel))
		return;

	Reset();
	CReadPacketState backup = {};
	backup.Store();

	Hooks::CL_ReadPackets::Hook.Original<Hooks::CL_ReadPackets::fn>()(bFinalTick);

	m_State.Store();

	backup.Restore();
	m_ReadGate.Arm(nFrame, pNetChannel);
}

bool CNetworkFix::ShouldReadPackets()
{
	if (!CFG::Misc_Ping_Reducer || !I::EngineClient || !I::ClientState || !I::GlobalVars || !I::EngineClient->IsInGame())
	{
		Reset();
		return true;
	}

	const auto pNetChannel = I::EngineClient->GetNetChannelInfo();
	if (!pNetChannel || pNetChannel->IsLoopback())
	{
		Reset();
		return true;
	}

	if (!m_ReadGate.Consume(I::GlobalVars->framecount, pNetChannel))
	{
		m_State = {};
		return true;
	}

	m_State.Restore();
	m_State = {};

	return false;
}

void CNetworkFix::Reset()
{
	m_State = {};
	m_ReadGate.Reset();
}
