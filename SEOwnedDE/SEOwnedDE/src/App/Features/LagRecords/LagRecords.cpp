#include "LagRecords.h"

#include "../CFG.h"

int CLagRecords::PlayerToIndex(C_TFPlayer* pPlayer)
{
	if (!pPlayer)
		return -1;

	const int idx = pPlayer->entindex();

	if (idx < 1 || idx >= MAX_PLAYERS)
		return -1;

	return idx;
}

float CLagRecords::GetOutgoingLatency()
{
	if (auto pNet = I::EngineClient->GetNetChannelInfo())
		return pNet->GetLatency(FLOW_OUTGOING);

	return 0.0f;
}

bool CLagRecords::IsSimulationTimeValid(float flCurSimTime, float flCmprSimTime)
{
	const int nWindow = CFG::LagRecords_BacktrackWindow;

	if (nWindow <= 0)
		return false;

	float flMaxWindow = nWindow / 1000.0f;

	static ConVar* sv_maxunlag = I::CVar->FindVar("sv_maxunlag");

	if (sv_maxunlag)
	{
		const float flUnlag = sv_maxunlag->GetFloat();

		if (flUnlag > 0.0f && flMaxWindow > flUnlag)
			flMaxWindow = flUnlag;
	}

	if (flCmprSimTime > flCurSimTime)
		return false;

	const float flDelta = flCurSimTime - flCmprSimTime;

	if (flDelta >= flMaxWindow)
	{
		const float flLatency = GetOutgoingLatency() + SDKUtils::GetLerp();

		if (flLatency > 0.0f)
		{
			const float flCorrected = flDelta - flLatency;

			if (flCorrected < flMaxWindow && flCorrected > -0.2f)
				return true;
		}

		return false;
	}

	return true;
}

void CLagRecords::AddRecord(C_TFPlayer* pPlayer)
{
	if (!pPlayer || CFG::LagRecords_BacktrackWindow <= 0)
		return;

	if (pPlayer->IsDormant())
		return;

	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return;

	LagRecord_t newRecord = {};

	m_bSettingUpBones = true;

	{
		auto it = m_FailedChildBones.begin();
		while (it != m_FailedChildBones.end())
		{
			auto* child = *it;
			if (!child || !child->GetMoveParent() || child->GetMoveParent() != pPlayer)
			{
				it = m_FailedChildBones.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	const auto setup_bones_optimization{ CFG::Misc_SetupBones_Optimization };

	if (setup_bones_optimization)
	{
		pPlayer->InvalidateBoneCache();
	}

	const auto result = pPlayer->SetupBones(newRecord.BoneMatrix, MAX_BONE_COUNT, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime);

	if (setup_bones_optimization)
	{
		auto attach = pPlayer->FirstMoveChild();
		while (attach)
		{
			if (attach->ShouldDraw())
			{
				attach->InvalidateBoneCache();
				const auto childResult = attach->SetupBones(nullptr, -1, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime);

				if (!childResult)
				{
					m_FailedChildBones.insert(attach);
				}
			}

			attach = attach->NextMovePeer();
		}
	}

	m_bSettingUpBones = false;

	if (!result)
		return;

	newRecord.Player = pPlayer;
	newRecord.SimulationTime = pPlayer->m_flSimulationTime();
	newRecord.AbsOrigin = pPlayer->GetAbsOrigin();
	newRecord.AbsAngles = pPlayer->GetAbsAngles();
	newRecord.EyeAngles = pPlayer->GetEyeAngles();
	newRecord.Velocity = pPlayer->m_vecVelocity();
	newRecord.Center = pPlayer->GetCenter();
	newRecord.Flags = pPlayer->m_fFlags();

	if (const auto pAnimState = pPlayer->GetAnimState())
		newRecord.FeetYaw = pAnimState->m_flCurrentFeetYaw;

	newRecord.MasterSequence = pPlayer->m_nSequence();
	newRecord.MasterCycle = pPlayer->m_flCycle();

	auto& records = m_LagRecords[idx];

	if (!records.empty())
	{
		const auto& front = records.front();

		if (newRecord.SimulationTime <= front.SimulationTime)
			return;

		if ((newRecord.AbsOrigin - front.AbsOrigin).LengthSqr() > LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR)
			newRecord.bTeleported = true;
	}

	records.emplace_front(newRecord);

	if (records.size() > MAX_LAG_RECORDS)
		records.pop_back();
}

const LagRecord_t* CLagRecords::GetRecord(C_TFPlayer* pPlayer, int nRecord)
{
	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return nullptr;

	const auto& records = m_LagRecords[idx];
	if (nRecord < 0 || nRecord >= static_cast<int>(records.size()))
		return nullptr;

	return &records[nRecord];
}

bool CLagRecords::HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords)
{
	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return false;

	const size_t nSize = m_LagRecords[idx].size();
	if (nSize == 0)
		return false;

	if (pTotalRecords)
		*pTotalRecords = static_cast<int>(nSize);

	return true;
}

void CLagRecords::UpdateRecords()
{
	const auto pLocal = H::Entities->GetLocal();

	if (!pLocal || pLocal->deadflag() || pLocal->InCond(TF_COND_HALLOWEEN_GHOST_MODE) || pLocal->InCond(TF_COND_HALLOWEEN_KART))
	{
		for (auto& records : m_LagRecords)
			records.clear();

		if (!m_FailedChildBones.empty())
		{
			m_FailedChildBones.clear();
		}

		return;
	}

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
	{
		if (!pEntity || pEntity == pLocal)
		{
			continue;
		}

		const auto pPlayer = pEntity->As<C_TFPlayer>();

		if (pPlayer->deadflag())
		{
			const int idx = PlayerToIndex(pPlayer);
			if (idx >= 0)
				m_LagRecords[idx].clear();
		}
	}

	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		auto& records = m_LagRecords[i];

		for (auto recIt = records.begin(); recIt != records.end(); )
		{
			if (!recIt->Player || !IsSimulationTimeValid(recIt->Player->m_flSimulationTime(), recIt->SimulationTime))
			{
				recIt = records.erase(recIt);
			}
			else
			{
				++recIt;
			}
		}
	}
}

LagRecordCachedState_t CLagRecords::CacheCurrentState(C_TFPlayer* pPlayer)
{
	LagRecordCachedState_t state = {};
	state.AbsOrigin = pPlayer->GetAbsOrigin();
	state.EyeAngles = pPlayer->GetEyeAngles();
	state.Flags = pPlayer->m_fFlags();

	if (const auto pAnimState = pPlayer->GetAnimState())
		state.FeetYaw = pAnimState->m_flCurrentFeetYaw;

	return state;
}

bool CLagRecords::DiffersFromCurrentCached(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached)
{
	if ((cached.AbsOrigin - pRecord->AbsOrigin).LengthSqr() > 0.01f)
		return true;

	const float flYawDelta = std::remainderf(cached.EyeAngles.y - pRecord->EyeAngles.y, 360.0f);
	if (fabsf(flYawDelta) > 0.1f)
		return true;

	const float flPitchDelta = std::remainderf(cached.EyeAngles.x - pRecord->EyeAngles.x, 360.0f);
	if (fabsf(flPitchDelta) > 0.1f)
		return true;

	const float flRollDelta = std::remainderf(cached.EyeAngles.z - pRecord->EyeAngles.z, 360.0f);
	if (fabsf(flRollDelta) > 0.1f)
		return true;

	if (cached.Flags != pRecord->Flags)
		return true;

	if (fabsf(cached.FeetYaw - pRecord->FeetYaw) > 0.1f)
		return true;

	return false;
}

bool CLagRecords::DiffersFromCurrent(const LagRecord_t* pRecord)
{
	const auto pPlayer = pRecord->Player;

	if (!pPlayer)
		return false;

	return DiffersFromCurrentCached(pRecord, CacheCurrentState(pPlayer));
}

const LagRecord_t* CLagRecords::FindInterpolatedRecord(C_TFPlayer* pPlayer, float flTargetTime, LagRecord_t& outRecord)
{
	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return nullptr;

	const auto& records = m_LagRecords[idx];
	if (records.empty())
		return nullptr;

	const LagRecord_t* prevRecord = nullptr;
	const LagRecord_t* record = nullptr;

	for (int i = 0; i < static_cast<int>(records.size()); ++i)
	{
		const auto& rec = records[i];

	if (rec.bTeleported)
			continue;

		prevRecord = record;
		record = &rec;

		if (rec.SimulationTime <= flTargetTime)
			break;
	}

	if (!record)
		return nullptr;

	if (prevRecord && record->SimulationTime < flTargetTime && prevRecord->SimulationTime > record->SimulationTime)
	{
		const float dt = prevRecord->SimulationTime - record->SimulationTime;

		if (dt > 1e-4f)
		{
			const float frac = std::clamp((flTargetTime - record->SimulationTime) / dt, 0.0f, 1.0f);

			outRecord = *record;
			outRecord.AbsOrigin = record->AbsOrigin + (prevRecord->AbsOrigin - record->AbsOrigin) * frac;
			outRecord.AbsAngles = record->AbsAngles + (prevRecord->AbsAngles - record->AbsAngles) * frac;
			outRecord.EyeAngles = record->EyeAngles + (prevRecord->EyeAngles - record->EyeAngles) * frac;
			outRecord.Center = record->Center + (prevRecord->Center - record->Center) * frac;
			outRecord.SimulationTime = flTargetTime;

			for (int b = 0; b < MAX_BONE_COUNT; ++b)
			{
				for (int col = 0; col < 3; ++col)
				{
					for (int row = 0; row < 4; ++row)
					{
						outRecord.BoneMatrix[b][col][row] = record->BoneMatrix[b][col][row] + (prevRecord->BoneMatrix[b][col][row] - record->BoneMatrix[b][col][row]) * frac;
					}
				}
			}

			return &outRecord;
		}
	}

	return record;
}

void CLagRecordMatrixHelper::Set(const LagRecord_t* pRecord)
{
	if (!pRecord)
		return;

	const auto pPlayer = pRecord->Player;

	if (!pPlayer || pPlayer->deadflag())
		return;

	const auto pCachedBoneData = pPlayer->GetCachedBoneData();

	if (!pCachedBoneData)
		return;

	StackEntry_t entry;
	entry.Player = pPlayer;
	entry.AbsOrigin = pPlayer->GetAbsOrigin();
	entry.AbsAngles = pPlayer->GetAbsAngles();
	entry.BoneCount = std::min(pCachedBoneData->Count(), MAX_BONE_COUNT);
	memcpy(entry.BoneMatrix, pCachedBoneData->Base(), sizeof(matrix3x4_t) * entry.BoneCount);

	memcpy(pCachedBoneData->Base(), pRecord->BoneMatrix, sizeof(matrix3x4_t) * entry.BoneCount);
	pPlayer->SetAbsOrigin(pRecord->AbsOrigin);
	pPlayer->SetAbsAngles(pRecord->AbsAngles);

	m_Stack.push_back(entry);
	++m_nActiveDepth;
}

void CLagRecordMatrixHelper::Restore()
{
	if (m_Stack.empty() || m_nActiveDepth <= 0)
		return;

	const auto entry = m_Stack.back();
	m_Stack.pop_back();
	--m_nActiveDepth;

	if (!entry.Player)
		return;

	const auto pCachedBoneData = entry.Player->GetCachedBoneData();

	if (!pCachedBoneData)
		return;

	entry.Player->SetAbsOrigin(entry.AbsOrigin);
	entry.Player->SetAbsAngles(entry.AbsAngles);

	const int nBoneCount = std::min(pCachedBoneData->Count(), entry.BoneCount);
	memcpy(pCachedBoneData->Base(), entry.BoneMatrix, sizeof(matrix3x4_t) * nBoneCount);
}
