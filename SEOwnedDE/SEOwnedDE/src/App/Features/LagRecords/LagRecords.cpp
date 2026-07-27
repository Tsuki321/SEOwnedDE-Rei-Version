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

bool CLagRecords::AreConsumersActive()
{
	const bool bHitscan = CFG::Aimbot_Active
		&& CFG::Aimbot_Target_Players
		&& CFG::Aimbot_Hitscan_Active
		&& CFG::Aimbot_Hitscan_Target_LagRecords;
	const bool bMelee = CFG::Aimbot_Active
		&& CFG::Aimbot_Target_Players
		&& CFG::Aimbot_Melee_Active
		&& CFG::Aimbot_Melee_Target_LagRecords;
	const bool bProjectilePrediction = CFG::Aimbot_Active
		&& CFG::Aimbot_Target_Players
		&& CFG::Aimbot_Projectile_Active
		&& (CFG::Aimbot_Projectile_Ground_Strafe_Prediction
			|| CFG::Aimbot_Projectile_Air_Strafe_Prediction
			|| CFG::Aimbot_Projectile_Aim_Prediction_Method == 1);
	const bool bBackstab = CFG::Triggerbot_Active
		&& CFG::Triggerbot_AutoBackstab_Active
		&& CFG::Triggerbot_AutoBackstab_Use_LagRecords;
	const bool bHistoricalModels = CFG::Materials_Active
		&& CFG::Materials_Players_Active
		&& !CFG::Materials_Players_Ignore_LagRecords;

	return bHitscan || bMelee || bProjectilePrediction || bBackstab || bHistoricalModels;
}

bool CLagRecords::ShouldCaptureRecord(C_TFPlayer* pLocal, C_TFPlayer* pPlayer)
{
	if (!pLocal || !pPlayer || pPlayer == pLocal || pPlayer->IsDormant() || pPlayer->deadflag())
		return false;

	if (pPlayer->m_iTeamNum() == pLocal->m_iTeamNum())
		return false;

	if (!AreConsumersActive())
		return false;

	return !CFG::Misc_LagRecords_Skip_Offscreen
		|| F::VisualUtils->IsOnScreenNoEntity(pLocal, pPlayer->GetAbsOrigin());
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

void CLagRecords::AddRenderRecord(C_TFPlayer* pPlayer, float flPoseTime)
{
	if (!pPlayer || !I::GlobalVars || !std::isfinite(flPoseTime) || flPoseTime <= 0.0f)
		return;

	if (pPlayer->IsDormant() || pPlayer->deadflag())
		return;

	const int idx = PlayerToIndex(pPlayer);
	if (idx < 0)
		return;

	auto& records = m_LagRecords[idx];

	// Validate against the head record (newest in the ring) BEFORE the
	// expensive SetupBones so a non-advancing tick bails without touching the
	// ring. SimulationTime, origin and identity are all readable without bones,
	// so the only work an early-out wastes is a handful of netvar reads (the
	// original ran SetupBones first, then discarded its result here).
	if (flPoseTime > pPlayer->m_flSimulationTime() + 0.001f)
		return;

	const float flSimTime = flPoseTime;
	const Vec3 vecOrigin = pPlayer->GetAbsOrigin();
	const int nModelIndex = pPlayer->m_nModelIndex();
	size_t baseCount = m_RecordCounts[idx];
	bool bTeleported = false;

	if (baseCount > 0)
	{
		const auto& head = records[m_RecordHeads[idx]];

		if (head.Player != pPlayer)
		{
			// Stale player identity (entity index recycled): drop the ring so we
			// start writing from a clean slot.
			m_RecordCounts[idx] = 0u;
			baseCount = 0;
		}
		else
		{
			if (TIME_TO_TICKS(flSimTime) <= TIME_TO_TICKS(head.SimulationTime))
				return;

			// Teleport detection scaled by elapsed time and the target's last
			// known velocity. A fixed 64u gate flags legitimate movement as a
			// teleport whenever consecutive records span more than one tick -
			// e.g. a choking / fakelagging target whose records land many ticks
			// apart, or a fast mover across a packet-loss gap - and drops those
			// otherwise-valid backtrack records. A real teleport (teleporter,
			// Eureka, respawn) moves the player with no matching velocity, so it
			// still trips the gate. Allowance = base radius + velocity * dt with
			// slack for acceleration / air-strafe; compared in squared space so
			// the sqrt below only runs on the rare >base-radius case.
			const float flMovedSqr = (vecOrigin - head.AbsOrigin).LengthSqr();

			if (flMovedSqr > LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR)
			{
				const float flDt = flSimTime - head.SimulationTime; // > 0 (checked above)
				const float flAllowed = LAG_COMPENSATION_TELEPORTED_BASE_RADIUS
					+ head.Velocity.Length() * flDt * LAG_COMPENSATION_TELEPORTED_VELOCITY_SLACK;

				if (flMovedSqr > flAllowed * flAllowed)
					bTeleported = true;
			}
		}
	}

	// Write straight into the ring slot one past the current head. Aliasing the
	// destination (instead of a stack-local record that is later move-assigned
	// in) lets SetupBones land the 128-matrix bone block in its final home just
	// once - no 6 KB zero-init of BoneData and no 6 KB copy on commit.
	const size_t newHead = (m_RecordHeads[idx] + 1) % MAX_LAG_RECORDS;
	LagRecord_t& newRecord = records[newHead];

	// Capture the already-interpolated, already-animated render pose. Do not
	// invalidate the cache here: SetupBones can reuse or complete the coherent
	// vanilla pose, and normal rendering remains free of a second animation pass.
	const bool bHistoricalModels = CFG::Materials_Active
		&& CFG::Materials_Players_Active
		&& !CFG::Materials_Players_Ignore_LagRecords;
	const int nBoneMask = bHistoricalModels ? BONE_USED_BY_ANYTHING : BONE_USED_BY_HITBOX;

	const bool bCaptured = pPlayer->SetupBones(
		newRecord.BoneData.data(),
		MAX_BONE_COUNT,
		nBoneMask,
		I::GlobalVars->curtime
	);

	int nCapturedBoneCount = 0;
	if (bCaptured)
	{
		if (const auto pCachedBoneData = pPlayer->GetCachedBoneData())
			nCapturedBoneCount = std::clamp(pCachedBoneData->Count(), 0, MAX_BONE_COUNT);
	}

	if (!bCaptured || nCapturedBoneCount <= 0)
	{
		// SetupBones may have partially overwritten the slot's bone block. When
		// the ring was full this slot was the oldest committed record, so shrink
		// the count by one to retire it; otherwise the slot was uncommitted
		// scratch and the count is left untouched. Either way head does not
		// advance, so the aborted write is never surfaced by GetRecord.
		if (m_RecordCounts[idx] >= MAX_LAG_RECORDS)
			m_RecordCounts[idx] = MAX_LAG_RECORDS - 1;
		return;
	}

	newRecord.Player = pPlayer;
	newRecord.ModelIndex = nModelIndex;
	newRecord.SimulationTime = flSimTime;
	newRecord.AbsOrigin = vecOrigin;
	newRecord.AbsAngles = pPlayer->GetAbsAngles();
	newRecord.EyeAngles = pPlayer->GetEyeAngles();
	newRecord.Velocity = pPlayer->m_vecVelocity();
	newRecord.Center = pPlayer->GetCenter();
	newRecord.Flags = pPlayer->m_fFlags();
	newRecord.bTeleported = bTeleported;

	// Reused slot: default every conditionally-written field so stale values
	// from the record being overwritten can never leak through.
	newRecord.FeetYaw = 0.0f;
	if (const auto pAnimState = pPlayer->GetAnimState())
		newRecord.FeetYaw = pAnimState->m_flCurrentFeetYaw;

	newRecord.BoneCount = nCapturedBoneCount;

	// Commit: advance the head to the freshly written slot and grow the count
	// (clamped at capacity, oldest record silently retired on overflow).
	m_RecordHeads[idx] = static_cast<uint8_t>(newHead);
	m_RecordCounts[idx] = static_cast<uint8_t>(std::min<size_t>(baseCount + 1, MAX_LAG_RECORDS));
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
		m_flSmoothedLatency = -1.0f;

		return;
	}

	if (!AreConsumersActive())
	{
		m_RecordCounts.fill(0u);
		m_CachedStates = {};
		m_flSmoothedLatency = -1.0f;
		return;
	}

	for (const auto pEntity : H::Entities->GetGroup(EEntGroup::PLAYERS_ALL))
	{
		if (!pEntity || pEntity == pLocal)
		{
			continue;
		}

		const auto pPlayer = pEntity->As<C_TFPlayer>();
		const int idx = PlayerToIndex(pPlayer);

		if (pPlayer->deadflag() || pPlayer->m_iTeamNum() == pLocal->m_iTeamNum())
		{
			if (idx >= 0)
			{
				m_RecordCounts[idx] = 0u;
				m_CachedStates[idx] = {};
			}
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
	// Exponentially smooth the outgoing latency so an unstable connection that
	// alternates high/low ping does not thrash the window and repeatedly discard
	// deep records on the low-ping frames (tail pruning below is one-way). Lerp
	// is convar-derived and already stable, so only the network term is smoothed.
	const float flRawLatency = GetOutgoingLatency();
	if (m_flSmoothedLatency < 0.0f)
		m_flSmoothedLatency = flRawLatency;
	else
		m_flSmoothedLatency += (flRawLatency - m_flSmoothedLatency) * LAG_LATENCY_EMA_ALPHA;

	const float flLatency = m_flSmoothedLatency + SDKUtils::GetLerp();

	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		if (m_RecordCounts[i] == 0)
			continue;

		auto& records = m_LagRecords[i];
		const size_t head = m_RecordHeads[i];

		// Validate the stored Player pointer against the live entity list before
		// dereferencing it. The ring is keyed by entity index, so re-fetch
		// whatever entity currently occupies this slot and compare by pointer
		// (never dereferences the stored pointer). A raw C_TFPlayer* kept across
		// the unlag window can dangle after a disconnect/recycle and fault on the
		// reads below; on mismatch/dormancy/death, drop the ring.
		C_TFPlayer* pFirstPlayer = nullptr;
		if (const auto pClientEnt = I::ClientEntityList->GetClientEntity(i))
			pFirstPlayer = pClientEnt->As<C_TFPlayer>();

		if (!pFirstPlayer || pFirstPlayer->IsDormant() || pFirstPlayer->deadflag()
			|| pFirstPlayer != records[head].Player)
		{
			m_RecordCounts[i] = 0u;
			m_CachedStates[i] = {};
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
	//
	// Thresholds are relaxed to 0.5 deg / 0.5 units so that near-stationary
	// targets (scoped Sniper, revved Heavy) still offer usable backtrack
	// candidates instead of being silently filtered out. 0.1 deg / 0.1 units
	// was tight enough that every record of a barely-moving player was
	// rejected, forcing manual shots onto the interpolated live pose.
	if (cached.Flags != pRecord->Flags)
		return true;

	if (fabsf(cached.FeetYaw - pRecord->FeetYaw) > 0.5f)
		return true;

	if ((cached.AbsOrigin - pRecord->AbsOrigin).LengthSqr() > 0.25f)
		return true;

	// fmodf-based wrap into [-180, 180]. std::remainderf respects the IEEE
	// rounding mode and is several times slower than fmodf on MSVC.
	const float flYawDelta = std::fmodf(cached.EyeAngles.y - pRecord->EyeAngles.y + 540.0f, 360.0f) - 180.0f;
	if (fabsf(flYawDelta) > 0.5f)
		return true;

	const float flPitchDelta = std::fmodf(cached.EyeAngles.x - pRecord->EyeAngles.x + 540.0f, 360.0f) - 180.0f;
	if (fabsf(flPitchDelta) > 0.5f)
		return true;

	const float flRollDelta = std::fmodf(cached.EyeAngles.z - pRecord->EyeAngles.z + 540.0f, 360.0f) - 180.0f;
	return fabsf(flRollDelta) > 0.5f;
}

bool CLagRecords::IsRecordUsable(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached)
{
	// Order the cheapest rejects first: the null and teleport checks are single
	// loads/branches, so they short-circuit before the multi-field pose compare.
	if (!pRecord || !pRecord->Player || pRecord->bTeleported
		|| pRecord->ModelIndex != pRecord->Player->m_nModelIndex())
		return false;

	return DiffersFromCurrentCached(pRecord, cached);
}

bool CLagRecordMatrixHelper::Set(const LagRecord_t* pRecord)
{
	if (!pRecord || pRecord->BoneCount <= 0)
		return false;

	if (m_nActiveDepth >= MAX_MATRIX_HELPER_DEPTH)
		return false;

	const auto pPlayer = pRecord->Player;

	if (!pPlayer || pPlayer->deadflag())
		return false;

	if (pRecord->ModelIndex != pPlayer->m_nModelIndex())
		return false;

	const auto pCachedBoneData = pPlayer->GetCachedBoneData();

	if (!pCachedBoneData)
		return false;

	// Sanity-guard the cached bone count. GetCachedBoneData() resolves a
	// CUtlVector at a netvar-derived offset; a stale offset after a TF2 patch
	// makes Count() read garbage and turns the memcpys below into reads past
	// the bone buffer. Bail (no-op) instead of trusting it.
	const int nLiveCount = pCachedBoneData->Count();
	if (nLiveCount <= 0 || nLiveCount > MAX_BONE_COUNT)
		return false;

	const auto pLiveBones = pCachedBoneData->Base();
	if (!pLiveBones)
		return false;

	const int nApplyCount = std::min(nLiveCount, pRecord->BoneCount);
	if (nApplyCount <= 0)
		return false;

	auto& entry = m_Stack[m_nActiveDepth];
	entry.Player = pPlayer;
	entry.AbsOrigin = pPlayer->GetAbsOrigin();
	entry.AbsAngles = pPlayer->GetAbsAngles();
	entry.BoneCount = nLiveCount;
	entry.AppliedBoneCount = nApplyCount;
	entry.CachedBoneData = pCachedBoneData;
	memcpy(entry.BoneMatrix, pLiveBones, sizeof(matrix3x4_t) * entry.BoneCount);

	memcpy(pLiveBones, pRecord->BoneData.data(), sizeof(matrix3x4_t) * nApplyCount);

	pPlayer->SetAbsOrigin(pRecord->AbsOrigin);
	pPlayer->SetAbsAngles(pRecord->AbsAngles);

	++m_nActiveDepth;
	return true;
}

bool CLagRecordMatrixHelper::CopyActiveBones(C_BaseEntity* pEntity, matrix3x4_t* pBoneToWorldOut, int nMaxBones) const
{
	if (m_nActiveDepth <= 0 || !pEntity || !pBoneToWorldOut || nMaxBones <= 0)
		return false;

	const auto& entry = m_Stack[m_nActiveDepth - 1];
	if (entry.Player != pEntity || !entry.CachedBoneData)
		return false;

	const int nCachedCount = entry.CachedBoneData->Count();
	if (nCachedCount <= 0 || nCachedCount > MAX_BONE_COUNT)
		return false;

	const auto pCachedBones = entry.CachedBoneData->Base();
	if (!pCachedBones)
		return false;

	// Match C_BaseAnimating::SetupBones: callers either receive the complete
	// cached vector or a false result when their output buffer is too small.
	if (nCachedCount != entry.BoneCount || nMaxBones < nCachedCount)
		return false;

	memcpy(pBoneToWorldOut, pCachedBones, sizeof(matrix3x4_t) * nCachedCount);
	return true;
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

	// Sanity-guard (see Set): a stale bone-cache offset yields a bogus Count()
	// and the memcpy below would overrun. Bail instead.
	const int nCachedRestore = pCachedBoneData->Count();
	if (nCachedRestore <= 0 || nCachedRestore > MAX_BONE_COUNT)
		return;

	const auto pCachedBones = pCachedBoneData->Base();
	if (!pCachedBones)
		return;

	const int nBoneCount = std::min(nCachedRestore, entry.BoneCount);
	memcpy(pCachedBones, entry.BoneMatrix, sizeof(matrix3x4_t) * nBoneCount);
}
