#include "../../SDK/SDK.h"

#include <array>

MAKE_SIGNATURE(INetChannel_SendNetMsg, "engine.dll", "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 48 8B F1 45 0F B6 F1", 0x0);
MAKE_SIGNATURE(WriteUsercmd, "client.dll", "40 56 57 48 83 EC ? 41 8B 40", 0x0);

//credits: KGB

bool WriteUsercmdDeltaToBuffer(bf_write* buf, int from, int to)
{
	if (!buf || !I::Input)
		return false;

	CUserCmd nullcmd = {};
	CUserCmd* pFrom = &nullcmd;

	if (from != -1)
	{
		pFrom = I::Input->GetUserCmd(from);
		if (!pFrom)
			return false;
	}

	CUserCmd* pTo = I::Input->GetUserCmd(to);
	if (!pTo)
		return false;

	const auto pWriteUsercmd = Signatures::WriteUsercmd.Get();
	if (!pWriteUsercmd)
		return false;

	reinterpret_cast<void(__cdecl *)(bf_write*, CUserCmd*, CUserCmd*)>(pWriteUsercmd)(buf, pTo, pFrom);

	return !buf->m_bOverflow;
}

MAKE_HOOK(INetChannel_SendNetMsg, Signatures::INetChannel_SendNetMsg.Get(), bool, __fastcall,
	CNetChannel* pNet, INetMessage& msg, bool bForceReliable, bool bVoice)
{
	if (msg.GetType() != clc_Move || !Shifting::bShifting)
		return CALL_ORIGINAL(pNet, msg, bForceReliable, bVoice);

	if (!pNet || !I::ClientState || !I::Input)
		return CALL_ORIGINAL(pNet, msg, bForceReliable, bVoice);

	const int nChokedCommands = I::ClientState->chokedcommands;
	if (nChokedCommands < MAX_NEW_COMMANDS || nChokedCommands >= MAX_COMMANDS)
		return CALL_ORIGINAL(pNet, msg, bForceReliable, bVoice);

	const int nRequestedNewCommands = nChokedCommands + 1;
	const int nExtraCommands = nRequestedNewCommands - MAX_NEW_COMMANDS;
	if (pNet->m_nChokedPackets < nExtraCommands)
		return CALL_ORIGINAL(pNet, msg, bForceReliable, bVoice);

	// Keep the engine-built payload available as a fallback until the shifted
	// replacement has been serialized completely.
	const auto pMsg = reinterpret_cast<CLC_Move*>(&msg);
	CLC_Move shiftedMsg = *pMsg;
	alignas(4) std::array<unsigned char, 4000> shiftedData = {};
	shiftedMsg.m_DataOut.StartWriting(shiftedData.data(), static_cast<int>(shiftedData.size()));
	if (shiftedMsg.m_DataOut.GetData() != shiftedData.data())
		return CALL_ORIGINAL(pNet, msg, bForceReliable, bVoice);

	shiftedMsg.m_nNewCommands = MAX_NEW_COMMANDS;
	shiftedMsg.m_nBackupCommands = std::clamp(std::max(2, nExtraCommands), 0, MAX_BACKUP_COMMANDS);

	const int nNextCommandNr = I::ClientState->lastoutgoingcommand + nChokedCommands + 1;
	const int nNumCmds = shiftedMsg.m_nNewCommands + shiftedMsg.m_nBackupCommands;
	int nFrom = -1;

	for (int nTo = nNextCommandNr - nNumCmds + 1; nTo <= nNextCommandNr; nTo++)
	{
		if (!WriteUsercmdDeltaToBuffer(&shiftedMsg.m_DataOut, nFrom, nTo))
			return CALL_ORIGINAL(pNet, msg, bForceReliable, bVoice);

		nFrom = nTo;
	}

	const int nOldChokedPackets = pNet->m_nChokedPackets;
	pNet->m_nChokedPackets = nOldChokedPackets - nExtraCommands;

	const bool bSent = CALL_ORIGINAL(pNet, reinterpret_cast<INetMessage&>(shiftedMsg), bForceReliable, bVoice);
	if (!bSent)
		pNet->m_nChokedPackets = nOldChokedPackets;

	return bSent;
}
