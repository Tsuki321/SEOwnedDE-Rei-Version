#pragma once

#include "../../../SDK/SDK.h"

#include <array>
#include <cstdint>

// MAX_LAG_RECORDS: 24 ticks ~= 360 ms at 66 tick (240 ms at 100 tick). Sits
// comfortably inside the default sv_maxunlag (1.0 s) window, so essentially the
// whole ring stays valid for realistic pings (30-100 ms) - past the Materials
// ghost-render cap and the 5-record cap used by MovementSimulation + Aimbot
// hitscan/melee, while giving up only the deep reach that sv_maxunlag would
// truncate anyway. Per-player static storage is ~144 KB per slot (~4.6 MB
// across MAX_PLAYERS=33), dominated by the inline BoneData block.
inline constexpr int MAX_BONE_COUNT = 128;
inline constexpr int MAX_LAG_RECORDS = 24;
// MAX_MATRIX_HELPER_DEPTH: measured max nesting is 1 across all consumers
// (AimbotHitscan / AimbotMelee / AutoBackstab / Materials). 2 is one above
// the measured depth as a safety margin; cuts the matrix helper's static
// footprint from ~50 KB to ~12.5 KB.
inline constexpr int MAX_MATRIX_HELPER_DEPTH = 2;

inline constexpr float LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR = 64.0f * 64.0f;
// Base radius (units) below which a per-record displacement is never treated as
// a teleport - equals sqrt(LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR). Above it,
// the allowance grows by velocity * dt so multi-tick / choke gaps and fast
// movers are not misclassified as teleports.
inline constexpr float LAG_COMPENSATION_TELEPORTED_BASE_RADIUS = 64.0f;
// Slack multiplier on the velocity-derived displacement term. > 1 to absorb
// intra-interval acceleration and air-strafing without flagging real movement.
inline constexpr float LAG_COMPENSATION_TELEPORTED_VELOCITY_SLACK = 1.5f;

// EMA weight for the per-frame latency term that scopes the validity window.
// Small alpha => heavy smoothing (~1/alpha frames of memory, ~130 ms at 66 fps)
// so an unstable connection alternating between high and low ping does not
// oscillate the window and destructively truncate deep records on the low
// frames. Tail pruning in UpdateRecords is one-way (counts shrink and never
// grow back into overwritten slots), which is why the input needs smoothing.
inline constexpr float LAG_LATENCY_EMA_ALPHA = 0.12f;

struct LagRecord_t
{
	C_TFPlayer* Player = nullptr;
	int ModelIndex = -1;
	int BoneCount = 0;
	std::array<matrix3x4_t, MAX_BONE_COUNT> BoneData{};
	// Render records store the pose time paired with BoneData. GetCommandTick
	// preserves that pairing when a consumer writes a user command.
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
	// Heads/counts use uint8_t since MAX_LAG_RECORDS (24) fits trivially; cuts
	// per-player ring state from 16 B to 2 B and improves cache locality when
	// walking all MAX_PLAYERS rings in UpdateRecords.
	std::array<std::array<LagRecord_t, MAX_LAG_RECORDS>, MAX_PLAYERS> m_LagRecords = {};
	std::array<uint8_t, MAX_PLAYERS> m_RecordHeads = {};
	std::array<uint8_t, MAX_PLAYERS> m_RecordCounts = {};

	// Exponentially-smoothed latency (outgoing + lerp) used to scope the
	// per-frame validity window. Seeded to -1 so the first UpdateRecords adopts
	// the raw sample verbatim instead of easing up from zero; reset to -1 on
	// every full-ring clear (death / ghost / kart) so a stale average from a
	// prior life or server cannot leak into a fresh one.
	float m_flSmoothedLatency = -1.0f;

	// Per-player snapshot of the live (current-frame) state used by
	// DiffersFromCurrentCached. Built once per frame inside UpdateRecords so
	// that all consumers (AimbotHitscan, AimbotMelee, AutoBackstab, Materials)
	// share the same GetAbsOrigin / GetEyeAngles / m_fFlags / GetAnimState
	// results instead of redundantly re-fetching them per pass.
	std::array<LagRecordCachedState_t, MAX_PLAYERS> m_CachedStates = {};

	bool IsSimulationTimeValid(float flCurSimTime, float flCmprSimTime, float flMaxWindow, float flLatency);

	static int PlayerToIndex(C_TFPlayer* pPlayer);

public:
	static float GetOutgoingLatency();

	// Convert the pose time stored with a historical record into the command
	// tick the server uses for lag compensation.
	static int GetCommandTick(float flPoseTime)
	{
		return TIME_TO_TICKS(flPoseTime + SDKUtils::GetLerp());
	}

	// True when any feature that reads lag records is configured on. Used by
	// capture gating (FrameStageNotify) so the consumer CFG list lives in one
	// place instead of being copy-pasted across hook branches.
	static bool AreConsumersActive();

	// Whether AddRecord should run for this enemy under the current feature and
	// off-screen policy. Centralizing all validity gates keeps capture callers cheap.
	static bool ShouldCaptureRecord(C_TFPlayer* pLocal, C_TFPlayer* pPlayer);

	void AddRenderRecord(C_TFPlayer* pPlayer, float flPoseTime);
	const LagRecord_t* GetRecord(C_TFPlayer* pPlayer, int nRecord);
	bool HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords = nullptr);
	void UpdateRecords();
	static bool DiffersFromCurrentCached(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached);

	// Combined per-record usability gate shared by every backtrack consumer
	// (AimbotHitscan, AimbotMelee, AutoBackstab, Materials): a record is usable
	// when it is non-null, is not a post-teleport discontinuity, and its pose
	// actually differs from the player's live cached state (so we never
	// backtrack onto the interpolated present). Centralized so the call sites
	// cannot drift apart as the filter evolves.
	static bool IsRecordUsable(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached);

	// Returns the per-frame snapshot of the player's live state, populated
	// by UpdateRecords and reused by all consumers. Use this instead of
	// calling CacheCurrentState() per consumer pass. Guarded against a
	// hostile / recycled entindex so a bad caller index yields an empty
	// snapshot instead of an out-of-bounds read (the valid player range is
	// [1, MAX_PLAYERS), matching PlayerToIndex).
	const LagRecordCachedState_t& GetCachedState(int nPlayerIndex) const
	{
		if (nPlayerIndex < 1 || nPlayerIndex >= MAX_PLAYERS)
		{
			static const LagRecordCachedState_t kEmpty{};
			return kEmpty;
		}

		return m_CachedStates[nPlayerIndex];
	}

	static LagRecordCachedState_t CacheCurrentState(C_TFPlayer* pPlayer);
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
		int AppliedBoneCount = 0;
		CUtlVector<matrix3x4_t>* CachedBoneData = nullptr;
	};

	std::array<StackEntry_t, MAX_MATRIX_HELPER_DEPTH> m_Stack = {};
	int m_nActiveDepth = 0;

public:
	bool Set(const LagRecord_t* pRecord);
	void Restore();
	bool CopyActiveBones(C_BaseEntity* pEntity, matrix3x4_t* pBoneToWorldOut, int nMaxBones) const;
	bool IsActive() const { return m_nActiveDepth > 0; }
	bool IsActiveFor(const C_BaseEntity* pEntity) const
	{
		return IsActive()
			&& static_cast<const C_BaseEntity*>(m_Stack[m_nActiveDepth - 1].Player) == pEntity;
	}
};

MAKE_SINGLETON_SCOPED(CLagRecordMatrixHelper, LagRecordMatrixHelper, F);

class CLagRecordScope
{
	bool m_bActive = false;

public:
	explicit CLagRecordScope(const LagRecord_t* pRecord)
	{
		if (pRecord)
			m_bActive = F::LagRecordMatrixHelper->Set(pRecord);
	}

	~CLagRecordScope()
	{
		if (m_bActive)
			F::LagRecordMatrixHelper->Restore();
	}

	bool IsActive() const { return m_bActive; }

	CLagRecordScope(const CLagRecordScope&) = delete;
	CLagRecordScope& operator=(const CLagRecordScope&) = delete;
};
