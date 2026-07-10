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

	auto& records = m_LagRecords[idx];

	// Validate against the head record (newest in the ring) BEFORE the
	// expensive SetupBones so a non-advancing tick bails without touching the
	// ring. SimulationTime, origin and identity are all readable without bones,
	// so the only work an early-out wastes is a handful of netvar reads (the
	// original ran SetupBones first, then discarded its result here).
	const float flSimTime = pPlayer->m_flSimulationTime();
	const Vec3 vecOrigin = pPlayer->GetAbsOrigin();
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
			if (flSimTime <= head.SimulationTime)
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

	m_bSettingUpBones = true;

	const auto setup_bones_optimization{ CFG::Misc_SetupBones_Optimization };

	if (setup_bones_optimization)
	{
		pPlayer->InvalidateBoneCache();
	}

	// BoneData is an inline std::array<matrix3x4_t, MAX_BONE_COUNT> inside the
	// ring slot, so the buffer is already allocated. We narrow the authoritative
	// BoneCount after SetupBones succeeds based on the model's actual skeleton
	// size; matrices past that count are never read by any consumer.
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
		if (pLocal && F::VisualUtils->IsOnScreenNoEntity(pLocal, vecOrigin))
		{
			// Bound the peer walk: a corrupted/recycled move-peer chain could
			// otherwise cycle indefinitely and freeze the net-update thread. 64
			// is well above any realistic cosmetic/weapon count (typically <10).
			constexpr int MAX_MOVE_CHILDREN = 64;
			int nChild = 0;
			auto attach = pPlayer->FirstMoveChild();
			while (attach && nChild < MAX_MOVE_CHILDREN)
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

				++nChild;
				attach = attach->NextMovePeer();
			}
		}
	}

	m_bSettingUpBones = false;

	if (!result)
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

	// Authoritative bone count: bound by both the engine's cached count and
	// MAX_BONE_COUNT (the size we actually allocated). Defaults to 0 so a
	// missing cache leaves no consumer reading stale bones.
	newRecord.BoneCount = 0;
	if (const auto pCachedBoneData = pPlayer->GetCachedBoneData())
		newRecord.BoneCount = std::min(pCachedBoneData->Count(), MAX_BONE_COUNT);

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

	// fmodf-based wrap into [-180, 180] (matches NormalizeYawDelta in
	// CBaseAnimating_SetupBones.cpp). std::remainderf respects the IEEE
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
	if (!pRecord || pRecord->bTeleported)
		return false;

	return DiffersFromCurrentCached(pRecord, cached);
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

	// Sanity-guard the cached bone count. GetCachedBoneData() resolves a
	// CUtlVector at a netvar-derived offset; a stale offset after a TF2 patch
	// makes Count() read garbage and turns the memcpys below into reads past
	// the bone buffer. Bail (no-op) instead of trusting it.
	const int nLiveCount = pCachedBoneData->Count();
	if (nLiveCount <= 0 || nLiveCount > MAX_BONE_COUNT)
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

	// Sanity-guard (see Set): a stale bone-cache offset yields a bogus Count()
	// and the memcpy below would overrun. Bail instead.
	const int nCachedRestore = pCachedBoneData->Count();
	if (nCachedRestore <= 0 || nCachedRestore > MAX_BONE_COUNT)
		return;

	const int nBoneCount = std::min(nCachedRestore, entry.BoneCount);
	memcpy(pCachedBoneData->Base(), entry.BoneMatrix, sizeof(matrix3x4_t) * nBoneCount);
}
