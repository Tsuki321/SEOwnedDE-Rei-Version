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

bool CLagRecords::IsSimulationTimeValid(float flCurSimTime, float flCmprSimTime, float flMaxWindow, float flLatency)
{
	if (flCmprSimTime > flCurSimTime)
		return false;

	const float flDelta = flCurSimTime - flCmprSimTime;

	if (flDelta >= flMaxWindow)
	{
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
	if (!pPlayer)
		return;

	if (pPlayer->IsDormant())
		return;

	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return;

	LagRecord_t newRecord = {};

	m_bSettingUpBones = true;

	const auto setup_bones_optimization{ CFG::Misc_SetupBones_Optimization };

	if (setup_bones_optimization)
	{
		pPlayer->InvalidateBoneCache();
	}

	// Allocate the bone buffer once at MAX_BONE_COUNT; we narrow the
	// authoritative BoneCount after SetupBones succeeds based on the
	// model's actual skeleton size.
	newRecord.BoneData = std::make_unique<matrix3x4_t[]>(MAX_BONE_COUNT);
	const auto result = pPlayer->SetupBones(newRecord.BoneData.get(), MAX_BONE_COUNT, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime);

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

	// Authoritative bone count: bound by both the engine's cached count and
	// MAX_BONE_COUNT (the size we actually allocated).
	if (const auto pCachedBoneData = pPlayer->GetCachedBoneData())
		newRecord.BoneCount = std::min(pCachedBoneData->Count(), MAX_BONE_COUNT);

	auto& records = m_LagRecords[idx];
	const size_t oldCount = m_RecordCounts[idx];

	// Validate against the head record (newest in the ring) when one exists.
	if (oldCount > 0)
	{
		const auto& head = records[m_RecordHeads[idx]];

		if (head.Player != pPlayer)
		{
			// Stale player identity: drop every record for this slot so the
			// per-record BoneData allocations are released before reuse.
			for (auto& slot : records)
				slot = LagRecord_t{};
			m_RecordCounts[idx] = 0;
		}
		else
		{
			if (newRecord.SimulationTime <= head.SimulationTime)
				return;

			if ((newRecord.AbsOrigin - head.AbsOrigin).LengthSqr() > LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR)
				newRecord.bTeleported = true;
		}
	}

	// Move-construct into the ring slot one past the current head. The
	// unique_ptr ownership transfers, freeing the displaced record's
	// BoneData (if any) on overflow.
	const size_t newHead = (m_RecordHeads[idx] + 1) % MAX_LAG_RECORDS;
	records[newHead] = std::move(newRecord);

	m_RecordHeads[idx] = newHead;
	m_RecordCounts[idx] = std::min(m_RecordCounts[idx] + 1, static_cast<size_t>(MAX_LAG_RECORDS));
}

const LagRecord_t* CLagRecords::GetRecord(C_TFPlayer* pPlayer, int nRecord)
{
	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return nullptr;

	const size_t count = m_RecordCounts[idx];
	if (nRecord < 0 || nRecord >= static_cast<int>(count))
		return nullptr;

	// Translate logical index (0 = newest) into the physical ring slot.
	const size_t phys = (m_RecordHeads[idx] + MAX_LAG_RECORDS - nRecord) % MAX_LAG_RECORDS;
	return &m_LagRecords[idx][phys];
}

bool CLagRecords::HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords)
{
	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return false;

	const size_t count = m_RecordCounts[idx];
	if (count == 0)
		return false;

	if (pTotalRecords)
		*pTotalRecords = static_cast<int>(count);

	return true;
}

void CLagRecords::UpdateRecords()
{
	const auto pLocal = H::Entities->GetLocal();

	if (!pLocal || pLocal->deadflag() || pLocal->InCond(TF_COND_HALLOWEEN_GHOST_MODE) || pLocal->InCond(TF_COND_HALLOWEEN_KART))
	{
		for (int i = 0; i < MAX_PLAYERS; ++i)
		{
			for (auto& slot : m_LagRecords[i])
				slot = LagRecord_t{};
			m_RecordCounts[i] = 0;
		}

		if (!m_FailedChildBones.empty())
			m_FailedChildBones.clear();

		return;
	}

	{
		auto it = m_FailedChildBones.begin();
		while (it != m_FailedChildBones.end())
		{
			auto* child = *it;
			if (!child)
			{
				it = m_FailedChildBones.erase(it);
				continue;
			}

			const int childIdx = child->entindex();
			if (childIdx <= 0)
			{
				it = m_FailedChildBones.erase(it);
				continue;
			}

			const auto pVerify = I::ClientEntityList->GetClientEntity(childIdx);
			if (!pVerify || pVerify->As<C_BaseEntity>() != child)
			{
				it = m_FailedChildBones.erase(it);
				continue;
			}

			if (!child->GetMoveParent())
			{
				it = m_FailedChildBones.erase(it);
			}
			else
			{
				++it;
			}
		}
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
			{
				for (auto& slot : m_LagRecords[idx])
					slot = LagRecord_t{};
				m_RecordCounts[idx] = 0;
			}
		}
	}

	// Compute the lag-compensation window once per frame: the net channel
	// snapshot and the sv_maxunlag convar cannot change during this pass.
	float flMaxWindow = 1.0f;
	static ConVar* sv_maxunlag = I::CVar->FindVar("sv_maxunlag");
	if (sv_maxunlag)
	{
		const float flUnlag = sv_maxunlag->GetFloat();
		if (flUnlag > 0.0f)
			flMaxWindow = flUnlag;
	}
	const float flLatency = GetOutgoingLatency() + SDKUtils::GetLerp();

	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		if (m_RecordCounts[i] == 0)
			continue;

		auto& records = m_LagRecords[i];
		const size_t head = m_RecordHeads[i];

		// All records in this ring belong to the same player (AddRecord
		// invariant), so the live simtime / dormancy status are constant.
		C_TFPlayer* pFirstPlayer = records[head].Player;
		if (!pFirstPlayer || pFirstPlayer->IsDormant())
		{
			for (auto& slot : records)
				slot = LagRecord_t{};
			m_RecordCounts[i] = 0;
			continue;
		}

		const float flCurSimTime = pFirstPlayer->m_flSimulationTime();

		// Records are stored newest-first; SimulationTime strictly decreases
		// toward the back, and validity is monotonic w.r.t. age. Walk forward
		// to the first invalid record, then bulk-truncate the tail.
		size_t firstInvalid = m_RecordCounts[i];
		for (size_t n = 0; n < m_RecordCounts[i]; ++n)
		{
			const size_t phys = (head + MAX_LAG_RECORDS - n) % MAX_LAG_RECORDS;
			if (!IsSimulationTimeValid(flCurSimTime, records[phys].SimulationTime, flMaxWindow, flLatency))
			{
				firstInvalid = n;
				break;
			}
		}

		if (firstInvalid < m_RecordCounts[i])
		{
			// Release the BoneData allocations of the truncated records so
			// they don't linger in the ring slots until the next overwrite.
			for (size_t n = firstInvalid; n < m_RecordCounts[i]; ++n)
			{
				const size_t phys = (head + MAX_LAG_RECORDS - n) % MAX_LAG_RECORDS;
				records[phys] = LagRecord_t{};
			}
			m_RecordCounts[i] = firstInvalid;
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
	// Cheapest predicates first: int compare and a single fabsf short-circuit
	// before the more expensive vector / remainderf checks. Flag and feet-yaw
	// are the most common trip conditions on a freshly-ticked target.
	if (cached.Flags != pRecord->Flags)
		return true;

	if (fabsf(cached.FeetYaw - pRecord->FeetYaw) > 0.1f)
		return true;

	if ((cached.AbsOrigin - pRecord->AbsOrigin).LengthSqr() > 0.01f)
		return true;

	const float flYawDelta = std::remainderf(cached.EyeAngles.y - pRecord->EyeAngles.y, 360.0f);
	if (fabsf(flYawDelta) > 0.1f)
		return true;

	const float flPitchDelta = std::remainderf(cached.EyeAngles.x - pRecord->EyeAngles.x, 360.0f);
	if (fabsf(flPitchDelta) > 0.1f)
		return true;

	const float flRollDelta = std::remainderf(cached.EyeAngles.z - pRecord->EyeAngles.z, 360.0f);
	return fabsf(flRollDelta) <= 0.1f;
}

void CLagRecordMatrixHelper::Set(const LagRecord_t* pRecord)
{
	if (!pRecord)
		return;

	if (m_nActiveDepth >= MAX_MATRIX_HELPER_DEPTH)
		return; // Nested scope depth exceeded; bail out rather than overflow m_Stack.

	const auto pPlayer = pRecord->Player;

	if (!pPlayer || pPlayer->deadflag())
		return;

	const auto pCachedBoneData = pPlayer->GetCachedBoneData();

	if (!pCachedBoneData)
		return;

	auto& entry = m_Stack[m_nActiveDepth];
	entry.Player = pPlayer;
	entry.AbsOrigin = pPlayer->GetAbsOrigin();
	entry.AbsAngles = pPlayer->GetAbsAngles();
	entry.BoneCount = std::min(pCachedBoneData->Count(), MAX_BONE_COUNT);
	entry.CachedBoneData = pCachedBoneData;
	memcpy(entry.BoneMatrix, pCachedBoneData->Base(), sizeof(matrix3x4_t) * entry.BoneCount);

	// Apply the record's bones up to the min of (cached count, record count)
	// so we never read past the end of either buffer.
	const int nApplyCount = std::min(entry.BoneCount, pRecord->BoneCount);
	if (nApplyCount > 0 && pRecord->BoneData)
		memcpy(pCachedBoneData->Base(), pRecord->BoneData.get(), sizeof(matrix3x4_t) * nApplyCount);

	pPlayer->SetAbsOrigin(pRecord->AbsOrigin);
	pPlayer->SetAbsAngles(pRecord->AbsAngles);

	++m_nActiveDepth;
}

void CLagRecordMatrixHelper::Restore()
{
	if (m_nActiveDepth <= 0)
		return;

	--m_nActiveDepth;
	const auto& entry = m_Stack[m_nActiveDepth];

	if (!entry.Player)
		return;

	entry.Player->SetAbsOrigin(entry.AbsOrigin);
	entry.Player->SetAbsAngles(entry.AbsAngles);

	const auto pCachedBoneData = entry.CachedBoneData;

	if (!pCachedBoneData)
		return;

	const int nBoneCount = std::min(pCachedBoneData->Count(), entry.BoneCount);
	memcpy(pCachedBoneData->Base(), entry.BoneMatrix, sizeof(matrix3x4_t) * nBoneCount);
}
