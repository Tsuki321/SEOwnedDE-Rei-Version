#pragma once

#include "../../../SDK/SDK.h"

#include <array>
#include <cstdint>
#include <vector>

// MAX_LAG_RECORDS: 36 ticks ~= 545 ms at 66 tick. Comfortably above typical
// player ping and sv_maxunlag for most configs. Trimming from 66 cuts
// per-player storage by ~45% with no practical impact on lag-comp history.
inline constexpr int MAX_BONE_COUNT = 128;
inline constexpr int MAX_LAG_RECORDS = 36;
// MAX_MATRIX_HELPER_DEPTH: measured max nesting is 1 across all consumers
// (AimbotHitscan / AimbotMelee / AutoBackstab / Materials). 2 is one above
// the measured depth as a safety margin; cuts the matrix helper's static
// footprint from ~50 KB to ~12.5 KB.
inline constexpr int MAX_MATRIX_HELPER_DEPTH = 2;

inline constexpr float LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR = 64.0f * 64.0f;

struct LagRecord_t
{
	C_TFPlayer* Player = nullptr;
	int BoneCount = 0;
	std::array<matrix3x4_t, MAX_BONE_COUNT> BoneData{};
	float SimulationTime = -1.0f;
	Vec3 AbsOrigin = {};
	Vec3 AbsAngles = {};
	Vec3 EyeAngles = {};
	Vec3 Velocity = {};
	Vec3 Center = {};
	int Flags = 0;
	float FeetYaw = 0.0f;
	bool bTeleported = false;

	LagRecord_t() = default;
	LagRecord_t(LagRecord_t&&) noexcept = default;
	LagRecord_t& operator=(LagRecord_t&&) noexcept = default;
	LagRecord_t(const LagRecord_t&) = delete;
	LagRecord_t& operator=(const LagRecord_t&) = delete;
};

struct LagRecordCachedState_t
{
	Vec3 AbsOrigin = {};
	Vec3 EyeAngles = {};
	int Flags = 0;
	float FeetYaw = 0.0f;
};

class CLagRecords
{
	// Per-player fixed-capacity ring buffer. Logical index 0 is the newest
	// record; head points to the newest physical slot and the count clamps
	// at MAX_LAG_RECORDS, so the oldest record is silently overwritten on
	// overflow. With BoneData inlined into LagRecord_t, the entire history
	// is a single contiguous block per player - zero heap allocations, cache-
	// friendly iteration, and the SetupBones hot loop dereferences bones via
	// a direct pointer instead of unique_ptr::get().
	// Heads/counts use uint8_t since MAX_LAG_RECORDS=36 fits trivially; cuts
	// per-player ring state from 16 B to 2 B and improves cache locality when
	// walking all MAX_PLAYERS rings in UpdateRecords.
	std::array<std::array<LagRecord_t, MAX_LAG_RECORDS>, MAX_PLAYERS> m_LagRecords = {};
	std::array<uint8_t, MAX_PLAYERS> m_RecordHeads = {};
	std::array<uint8_t, MAX_PLAYERS> m_RecordCounts = {};
	bool m_bSettingUpBones = false;

	// Per-player snapshot of the live (current-frame) state used by
	// DiffersFromCurrentCached. Built once per frame inside UpdateRecords so
	// that all consumers (AimbotHitscan, AimbotMelee, AutoBackstab, Materials)
	// share the same GetAbsOrigin / GetEyeAngles / m_fFlags / GetAnimState
	// results instead of redundantly re-fetching them per pass.
	std::array<LagRecordCachedState_t, MAX_PLAYERS> m_CachedStates = {};

	// Failed-child wearable handles. Compacted in-place via swap-and-pop,
	// so the list is unsorted; consumer lookup is linear. Typical N is a
	// handful (cosmetic / weapon wearables that fail SetupBones capture),
	// so linear beats binary search here on cache locality alone. EHANDLE
	// staleness detection is cheap: Get() returns nullptr when the entity
	// is destroyed, and a handle value mismatch catches entity recycling
	// without a separate GetClientEntity round-trip.
	std::vector<CBaseHandle> m_FailedChildBones = {};

	bool IsSimulationTimeValid(float flCurSimTime, float flCmprSimTime, float flMaxWindow, float flLatency);

	static int PlayerToIndex(C_TFPlayer* pPlayer);

public:
	static float GetOutgoingLatency();
	void AddRecord(C_TFPlayer* pPlayer);
	const LagRecord_t* GetRecord(C_TFPlayer* pPlayer, int nRecord);
	bool HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords = nullptr);
	void UpdateRecords();
	static bool DiffersFromCurrentCached(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached);

	// Returns the per-frame snapshot of the player's live state, populated
	// by UpdateRecords and reused by all consumers. Use this instead of
	// calling CacheCurrentState() per consumer pass.
	const LagRecordCachedState_t& GetCachedState(int nPlayerIndex) const { return m_CachedStates[nPlayerIndex]; }

	static LagRecordCachedState_t CacheCurrentState(C_TFPlayer* pPlayer);
	bool IsSettingUpBones() { return m_bSettingUpBones; }

	bool HasFailedBones(C_BaseEntity* pEntity) const
	{
		if (m_FailedChildBones.empty())
			return false;

		// CBaseHandle has no converting ctor from IHandleEntity*; assign instead.
		CBaseHandle h;
		h = pEntity;
		return std::find(m_FailedChildBones.begin(), m_FailedChildBones.end(), h) != m_FailedChildBones.end();
	}
};

MAKE_SINGLETON_SCOPED(CLagRecords, LagRecords, F);

class CLagRecordMatrixHelper
{
	struct StackEntry_t
	{
		C_TFPlayer* Player = nullptr;
		Vec3 AbsOrigin = {};
		Vec3 AbsAngles = {};
		matrix3x4_t BoneMatrix[MAX_BONE_COUNT] = {};
		int BoneCount = 0;
		CUtlVector<matrix3x4_t>* CachedBoneData = nullptr;
	};

	std::array<StackEntry_t, MAX_MATRIX_HELPER_DEPTH> m_Stack = {};
	int m_nActiveDepth = 0;

public:
	void Set(const LagRecord_t* pRecord);
	void Restore();
	bool IsActive() const { return m_nActiveDepth > 0; }
};

MAKE_SINGLETON_SCOPED(CLagRecordMatrixHelper, LagRecordMatrixHelper, F);

class CLagRecordScope
{
	bool m_bActive = false;

public:
	explicit CLagRecordScope(const LagRecord_t* pRecord)
	{
		if (pRecord)
		{
			F::LagRecordMatrixHelper->Set(pRecord);
			m_bActive = F::LagRecordMatrixHelper->IsActive();
		}
	}

	~CLagRecordScope()
	{
		if (m_bActive)
			F::LagRecordMatrixHelper->Restore();
	}

	CLagRecordScope(const CLagRecordScope&) = delete;
	CLagRecordScope& operator=(const CLagRecordScope&) = delete;
};
