#include "LagRecords.h"

#include "../CFG.h"

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

	return flCurSimTime - flCmprSimTime < flMaxWindow;
}

void CLagRecords::AddRecord(C_TFPlayer* pPlayer)
{
	if (!pPlayer || CFG::LagRecords_BacktrackWindow <= 0)
		return;

	// Skip dormant players — they haven't received a network update this frame,
	// so their simulation time won't advance and we'd capture duplicate records.
	if (pPlayer->IsDormant())
		return;

	LagRecord_t newRecord = {};

	m_bSettingUpBones = true;

	// Phase 2: scope failed-child tracking per AddRecord call. Any wearable that
	// successfully refreshes below removes itself from the set; failures stay
	// tracked until the next AddRecord for this player.
	// Also purge any child whose root move-parent is no longer valid or no longer
	// matches the player we're about to process, to prevent stale pointers.
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

				// Phase 2: track wearables whose SetupBones returned false so the
				// cached-bone fast path can defer to the engine implementation
				// next frame and avoid serving a partial pose.
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

	auto& records = m_LagRecords[pPlayer];

	if (!records.empty() && newRecord.SimulationTime <= records.front().SimulationTime)
		return;

	records.emplace_front(newRecord);

	if (records.size() > MAX_LAG_RECORDS)
		records.pop_back();
}

const LagRecord_t* CLagRecords::GetRecord(C_TFPlayer* pPlayer, int nRecord)
{
	auto it = m_LagRecords.find(pPlayer);
	if (it == m_LagRecords.end())
		return nullptr;

	const auto& records = it->second;
	if (nRecord < 0 || nRecord >= static_cast<int>(records.size()))
		return nullptr;

	return &records[nRecord];
}

bool CLagRecords::HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords)
{
	auto it = m_LagRecords.find(pPlayer);
	if (it == m_LagRecords.end())
		return false;

	const size_t nSize = it->second.size();
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
		if (!m_LagRecords.empty())
		{
			m_LagRecords.clear();
		}

		// Phase 2: drop any tracked failed-child entries when the local player
		// is no longer in a valid state for record-keeping (map change, ghost,
		// kart, etc.) to prevent stale pointers persisting across respawn.
		if (!m_FailedChildBones.empty())
		{
			m_FailedChildBones.clear();
		}

		return;
	}

	// Remove invalid players
	for (const auto pEntity : H::Entities->GetGroup(CFG::Misc_SetupBones_Optimization ? EEntGroup::PLAYERS_ALL : EEntGroup::PLAYERS_ENEMIES))
	{
		if (!pEntity || pEntity == pLocal)
		{
			continue;
		}

		const auto pPlayer = pEntity->As<C_TFPlayer>();

		if (pPlayer->deadflag())
		{
			m_LagRecords.erase(pPlayer);
		}
	}

	// Single-pass: remove invalid records and empty player entries.
	for (auto it = m_LagRecords.begin(); it != m_LagRecords.end(); )
	{
		auto& records = it->second;

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

		if (records.empty())
			it = m_LagRecords.erase(it);
		else
			++it;
	}
}

bool CLagRecords::DiffersFromCurrent(const LagRecord_t* pRecord)
{
	const auto pPlayer = pRecord->Player;

	if (!pPlayer)
		return false;

	if ((pPlayer->GetAbsOrigin() - pRecord->AbsOrigin).LengthSqr() > 0.01f)
		return true;

	const Vec3 vCurEyeAngles = pPlayer->GetEyeAngles();

	const float flYawDelta = std::remainderf(vCurEyeAngles.y - pRecord->EyeAngles.y, 360.0f);
	if (fabsf(flYawDelta) > 0.1f)
		return true;

	const float flPitchDelta = std::remainderf(vCurEyeAngles.x - pRecord->EyeAngles.x, 360.0f);
	if (fabsf(flPitchDelta) > 0.1f)
		return true;

	const float flRollDelta = std::remainderf(vCurEyeAngles.z - pRecord->EyeAngles.z, 360.0f);
	if (fabsf(flRollDelta) > 0.1f)
		return true;

	if (pPlayer->m_fFlags() != pRecord->Flags)
		return true;

	if (const auto pAnimState = pPlayer->GetAnimState())
	{
		if (fabsf(pAnimState->m_flCurrentFeetYaw - pRecord->FeetYaw) > 0.1f)
			return true;
	}

	return false;
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

	// Pop the entry first so the stack stays consistent even if the player
	// or bone data became invalid between Set() and Restore().
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
