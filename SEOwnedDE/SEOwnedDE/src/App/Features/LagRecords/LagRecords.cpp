#include "LagRecords.h"

#include "../CFG.h"
#include "../VisualUtils/VisualUtils.h"

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

	// BoneData is now an inline std::array<matrix3x4_t, MAX_BONE_COUNT>
	// inside LagRecord_t, so the buffer is already allocated and zero-
	// initialized by the default constructor. We narrow the authoritative
	// BoneCount after SetupBones succeeds based on the model's actual
	// skeleton size.
	const auto result = pPlayer->SetupBones(newRecord.BoneData.data(), MAX_BONE_COUNT, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime);

	if (setup_bones_optimization)
	{
		// Visibility gate: when the player is fully off-screen, the
		// cosmetics' SetupBones output will be invisible until the
		// player comes back into view, at which point the next
		// AddRecord re-runs the loop. Skipping it here saves a
		// SetupBones call per move-child per off-screen player per
		// net tick. The player's own SetupBones above still runs
		// because lag records for off-screen players are still useful
		// when the player pops into view (Aimbot/Materials ghosts
		// need fresh bones).
		const auto pLocal = H::Entities->GetLocal();
		if (pLocal && F::VisualUtils->IsOnScreenNoEntity(pLocal, pPlayer->GetAbsOrigin()))
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
						// Insert if not already present. Lookup is linear because
						// the list is unsorted (swap-and-pop compact above) and
						// small in practice (typically a handful of failed
						// wearables).
						CBaseHandle h;
						h = attach;
						if (std::find(m_FailedChildBones.begin(), m_FailedChildBones.end(), h) == m_FailedChildBones.end())
							m_FailedChildBones.push_back(h);
					}
				}

				attach = attach->NextMovePeer();
			}
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
			// Stale player identity: the displaced record data will be
			// overwritten on next reuse; just reset the head so we start
			// writing from a clean slot.
			m_RecordCounts[idx] = 0u;
		}
		else
		{
			if (newRecord.SimulationTime <= head.SimulationTime)
				return;

			if ((newRecord.AbsOrigin - head.AbsOrigin).LengthSqr() > LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR)
				newRecord.bTeleported = true;
		}
	}

	// Move-construct into the ring slot one past the current head. With
	// BoneData now an inline std::array, the move-assign is a memcpy of
	// the 128 * 48-byte matrix block (the displaced slot is overwritten in
	// place on ring overflow).
	const size_t newHead = (m_RecordHeads[idx] + 1) % MAX_LAG_RECORDS;
	records[newHead] = std::move(newRecord);

	m_RecordHeads[idx] = static_cast<uint8_t>(newHead);
	m_RecordCounts[idx] = static_cast<uint8_t>(std::min<size_t>(m_RecordCounts[idx] + 1, MAX_LAG_RECORDS));
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
			m_RecordCounts[i] = 0u;

		m_CachedStates = {};

		if (!m_FailedChildBones.empty())
			m_FailedChildBones.clear();

		return;
	}

	{
		// Per-entry EHANDLE checks replace the original GetClientEntity +
		// cast-back round-trip. Get() returning nullptr means the entity is
		// gone; a handle value mismatch means the slot was recycled to a
		// different entity; missing move parent means the wearable is no
		// longer attached to the player. Compact via swap-and-pop since
		// the list is short and consumer lookup is linear (HasFailedBones
		// below) — O(1) per removal instead of the O(N) shift erase.
		size_t i = 0;
		while (i < m_FailedChildBones.size())
		{
			C_BaseEntity* pChild = static_cast<C_BaseEntity*>(m_FailedChildBones[i].Get());

			if (!pChild || !pChild->GetMoveParent())
			{
				m_FailedChildBones[i] = m_FailedChildBones.back();
				m_FailedChildBones.pop_back();
				continue;
			}

			CBaseHandle currentHandle;
			currentHandle = pChild;
			if (currentHandle != m_FailedChildBones[i])
			{
				m_FailedChildBones[i] = m_FailedChildBones.back();
				m_FailedChildBones.pop_back();
				continue;
			}

			++i;
		}
	}

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
	{
		if (!pEntity || pEntity == pLocal)
		{
			continue;
		}

		const auto pPlayer = pEntity->As<C_TFPlayer>();
		const int idx = PlayerToIndex(pPlayer);

		if (pPlayer->deadflag())
		{
			if (idx >= 0)
				m_RecordCounts[idx] = 0u;
			continue;
		}

		// Build the per-frame live-state snapshot once for all consumers
		// (AimbotHitscan, AimbotMelee, AutoBackstab, Materials) to share.
		// Safe because consumer passes run between this point and the next
		// FRAME_NET_UPDATE_START, during which the player's network-tracked
		// fields (AbsOrigin, EyeAngles, Flags) and animation-derived
		// FeetYaw do not change.
		if (idx >= 0)
		{
			auto& state = m_CachedStates[idx];
			state.AbsOrigin = pPlayer->GetAbsOrigin();
			state.EyeAngles = pPlayer->GetEyeAngles();
			state.Flags = pPlayer->m_fFlags();

			if (const auto pAnimState = pPlayer->GetAnimState())
				state.FeetYaw = pAnimState->m_flCurrentFeetYaw;
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
			m_RecordCounts[i] = 0u;
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
			m_RecordCounts[i] = static_cast<uint8_t>(firstInvalid);
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
	// before the more expensive vector / angle checks. Flag and feet-yaw
	// are the most common trip conditions on a freshly-ticked target.
	if (cached.Flags != pRecord->Flags)
		return true;

	if (fabsf(cached.FeetYaw - pRecord->FeetYaw) > 0.1f)
		return true;

	if ((cached.AbsOrigin - pRecord->AbsOrigin).LengthSqr() > 0.01f)
		return true;

	// fmodf-based wrap into [-180, 180] (matches NormalizeYawDelta in
	// CBaseAnimating_SetupBones.cpp). std::remainderf respects the IEEE
	// rounding mode and is several times slower than fmodf on MSVC.
	const float flYawDelta = std::fmodf(cached.EyeAngles.y - pRecord->EyeAngles.y + 540.0f, 360.0f) - 180.0f;
	if (fabsf(flYawDelta) > 0.1f)
		return true;

	const float flPitchDelta = std::fmodf(cached.EyeAngles.x - pRecord->EyeAngles.x + 540.0f, 360.0f) - 180.0f;
	if (fabsf(flPitchDelta) > 0.1f)
		return true;

	const float flRollDelta = std::fmodf(cached.EyeAngles.z - pRecord->EyeAngles.z + 540.0f, 360.0f) - 180.0f;
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
	if (nApplyCount > 0)
		memcpy(pCachedBoneData->Base(), pRecord->BoneData.data(), sizeof(matrix3x4_t) * nApplyCount);

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
