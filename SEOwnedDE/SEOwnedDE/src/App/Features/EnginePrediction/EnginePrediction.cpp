#include "EnginePrediction.h"

#include "../CFG.h"

int CEnginePrediction::GetTickbase(CUserCmd* pCmd, C_TFPlayer* pLocal)
{
	if (pCmd)
	{
		if (!m_pLastCmd || m_pLastCmd->hasbeenpredicted)
			m_nTick = pLocal->m_nTickBase();

		else m_nTick++;

		m_pLastCmd = pCmd;
	}

	return m_nTick;
}

void CEnginePrediction::Start(CUserCmd* pCmd)
{
	if (m_bActive)
		return;

	flags = 0;
	if (!pCmd || !I::MoveHelper || !I::GlobalVars || !I::Prediction || !I::GameMovement
		|| !I::EngineClient || !I::ClientEntityList)
		return;

	if (const auto pLocal = H::Entities->GetLocal())
	{
		flags = pLocal->m_fFlags();

		I::MoveHelper->SetHost(pLocal);
		pLocal->SetCurrentCommand(pCmd);

		static constexpr int MAX_INT = std::numeric_limits<int>::max();
		*SDKUtils::RandomSeed() = MD5_PseudoRandom(pCmd->command_number) & MAX_INT;

		m_fOldCurrentTime = I::GlobalVars->curtime;
		m_fOldFrameTime = I::GlobalVars->frametime;
		m_nOldTickCount = I::GlobalVars->tickcount;
		m_bActive = true;

		const int nOldTickBase = pLocal->m_nTickBase();
		const bool bOldIsFirstPrediction = I::Prediction->m_bFirstTimePredicted;
		const bool bOldInPrediction = I::Prediction->m_bInPrediction;

		const int nServerTicks = GetTickbase(pCmd, pLocal);

		I::GlobalVars->curtime = TICKS_TO_TIME(nServerTicks);
		I::GlobalVars->frametime = (I::Prediction->m_bEnginePaused ? 0.0f : TICK_INTERVAL);
		I::GlobalVars->tickcount = nServerTicks;

		I::Prediction->m_bFirstTimePredicted = false;
		I::Prediction->m_bInPrediction = true;

		I::Prediction->SetLocalViewAngles(pCmd->viewangles);

		I::Prediction->SetupMove(pLocal, pCmd, I::MoveHelper, &m_MoveData);
		I::GameMovement->ProcessMovement(pLocal, &m_MoveData);
		I::Prediction->FinishMove(pLocal, pCmd, &m_MoveData);

		pLocal->m_nTickBase() = nOldTickBase;

		I::Prediction->m_bInPrediction = bOldInPrediction;
		I::Prediction->m_bFirstTimePredicted = bOldIsFirstPrediction;

		if (H::Input->IsDown(CFG::Misc_Edge_Jump_Key))
		{
			if ((flags & FL_ONGROUND) && !(pLocal->m_fFlags() & FL_ONGROUND))
			{
				pCmd->buttons |= IN_JUMP;
			}
		}
	}
}

void CEnginePrediction::End()
{
	// Start may skip prediction. Never restore an unsaved clock or clear an
	// engine-owned command when this instance did not start a prediction pass.
	if (!m_bActive)
		return;

	m_bActive = false;
	if (I::GlobalVars)
	{
		I::GlobalVars->curtime = m_fOldCurrentTime;
		I::GlobalVars->frametime = m_fOldFrameTime;
		I::GlobalVars->tickcount = m_nOldTickCount;
	}

	if (I::MoveHelper)
		I::MoveHelper->SetHost(nullptr);

	if (I::EngineClient && I::ClientEntityList)
	{
		if (const auto pLocal = H::Entities->GetLocal())
			pLocal->SetCurrentCommand(nullptr);
	}
	*SDKUtils::RandomSeed() = -1;
}

void CEnginePrediction::Reset()
{
	End();
	m_pLastCmd = nullptr;
	m_nTick = 0;
	flags = 0;
}
