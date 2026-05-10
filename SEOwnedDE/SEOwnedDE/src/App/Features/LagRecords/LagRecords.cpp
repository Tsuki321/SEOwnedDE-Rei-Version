#include "LagRecords.h"

#include <ranges>

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
	if (CFG::LagRecords_BacktrackWindow <= 0)
		return;

	LagRecord_t newRecord = {};

	m_bSettingUpBones = true;

	// Phase 2: scope failed-child tracking per AddRecord call. Any wearable that
	// successfully refreshes below removes itself from the set; failures stay
	// tracked until the next AddRecord for this player.
	{
		auto it = m_FailedChildBones.begin();
		while (it != m_FailedChildBones.end())
		{
			auto* child = *it;
			if (!child || child->GetMoveParent() == pPlayer)
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

	const auto result = pPlayer->SetupBones(newRecord.BoneMatrix, 128, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime);

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
	newRecord.VecOrigin = pPlayer->m_vecOrigin();
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

	constexpr size_t MAX_RECORDS = 128;

	if (records.size() > MAX_RECORDS)
		records.pop_back();
}

const LagRecord_t* CLagRecords::GetRecord(C_TFPlayer* pPlayer, int nRecord, bool bSafe)
{
	if (!bSafe)
	{
		if (!m_LagRecords.contains(pPlayer))
			return nullptr;

		if (nRecord < 0 || nRecord > static_cast<int>(m_LagRecords[pPlayer].size() - 1))
			return nullptr;
	}
	else
	{
		if (!m_LagRecords.contains(pPlayer))
			return nullptr;

		const auto& records = m_LagRecords[pPlayer];
		if (nRecord < 0 || nRecord >= static_cast<int>(records.size()))
			return nullptr;
	}

	return &m_LagRecords[pPlayer][nRecord];
}

bool CLagRecords::HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords)
{
	if (m_LagRecords.contains(pPlayer))
	{
		const size_t nSize = m_LagRecords[pPlayer].size();

		if (nSize == 0)
			return false;

		if (pTotalRecords)
			*pTotalRecords = static_cast<int>(nSize);

		return true;
	}

	return false;
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

	// Remove invalid records
	for (auto& records : m_LagRecords | std::views::values)
	{
		for (auto it = records.begin(); it != records.end(); )
		{
			const auto& curRecord = *it;
			if (!curRecord.Player || !IsSimulationTimeValid(curRecord.Player->m_flSimulationTime(), curRecord.SimulationTime))
			{
				it = records.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	for (auto it = m_LagRecords.begin(); it != m_LagRecords.end(); )
	{
		if (it->second.empty())
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

	if ((pPlayer->GetEyeAngles() - pRecord->EyeAngles).Length() > 0.1f)
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

	m_pPlayer = pPlayer;
	m_vAbsOrigin = pPlayer->GetAbsOrigin();
	m_vAbsAngles = pPlayer->GetAbsAngles();
	memcpy(m_BoneMatrix, pCachedBoneData->Base(), sizeof(matrix3x4_t) * pCachedBoneData->Count());

	memcpy(pCachedBoneData->Base(), pRecord->BoneMatrix, sizeof(matrix3x4_t) * pCachedBoneData->Count());

	pPlayer->SetAbsOrigin(pRecord->AbsOrigin);
	pPlayer->SetAbsAngles(pRecord->AbsAngles);

	m_bSuccessfullyStored = true;
	m_bActive = true;
}

void CLagRecordMatrixHelper::Restore()
{
	if (!m_bSuccessfullyStored || !m_pPlayer)
		return;

	const auto pCachedBoneData = m_pPlayer->GetCachedBoneData();

	if (!pCachedBoneData)
		return;

	m_pPlayer->SetAbsOrigin(m_vAbsOrigin);
	m_pPlayer->SetAbsAngles(m_vAbsAngles);
	memcpy(pCachedBoneData->Base(), m_BoneMatrix, sizeof(matrix3x4_t) * pCachedBoneData->Count());

	m_pPlayer = nullptr;
	m_vAbsOrigin = {};
	m_vAbsAngles = {};
	std::memset(m_BoneMatrix, 0, sizeof(matrix3x4_t) * 128);
	m_bSuccessfullyStored = false;
	m_bActive = false;
}
