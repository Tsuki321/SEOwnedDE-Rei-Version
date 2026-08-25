#pragma once

#include "../../../SDK/SDK.h"

#include <array>
#include <cstdint>

// MAX_LAG_RECORDS: 24 ticks ~= 360 ms at 66 tick (240 ms at 100 tick). This is
// deliberately deeper than the usable backtrack window (LAG_MAX_BACKTRACK_TIME
// below) so that a frame which captures several ticks at once, or a target
// whose records land unevenly, still has candidates left inside the window
// after the age gate trims the tail. Do NOT read the ring depth as the reach of
// a shot - IsRecordUsable is what bounds that. Per-player static storage is
// ~144 KB per slot (~4.6 MB across MAX_PLAYERS=33), dominated by the inline
// BoneData block.
inline constexpr int MAX_BONE_COUNT = 128;
inline constexpr int MAX_LAG_RECORDS = 24;
// MAX_MATRIX_HELPER_DEPTH: measured max nesting is 1 across all consumers
// (AimbotHitscan / AimbotMelee / AutoBackstab / Materials). 2 is one above
// the measured depth as a safety margin; cuts the matrix helper's static
// footprint from ~50 KB to ~12.5 KB.
inline constexpr int MAX_MATRIX_HELPER_DEPTH = 2;

// Maximum age (seconds) of a record a shot may be rewound to.
//
// This is a server constraint, not a preference. CLagCompensationManager::
// StartLagCompensation computes its own target time from the player's measured
// latency + lerp and compares it against the time implied by cmd->tick_count;
// when the two deviate by more than 0.2 s it DISCARDS the client's tick and
// lag-compensates to its own estimate instead. A shot stamped with a deeper
// record therefore does not land on that record - it lands wherever the server
// decided the target was, i.e. roughly the present, which reads in-game as
// "the backtrack does nothing".
//
// sv_maxunlag (1.0 s) is a separate, much looser ceiling on how far the server
// will ever rewind. Validating against it alone - which is all UpdateRecords
// used to do - can never reject anything a 24-slot ring can hold, so every
// record was offered to the aimbot regardless of whether the server would
// honour it.
inline constexpr float LAG_MAX_BACKTRACK_TIME = 0.2f;

// Headroom withheld from that cutoff. LAG_MAX_BACKTRACK_TIME is a cliff, not a
// target: the server tests ITS OWN latency measurement against the correction
// our tick implies, and the two never agree exactly. A record sitting at 0.199 s
// is therefore a coin flip - honoured when the estimates happen to line up,
// silently discarded when they do not, which reads in-game as backtrack that
// works "sometimes". Spend a little depth to make the records we do offer land.
inline constexpr float LAG_BACKTRACK_SAFETY_MARGIN = 0.025f;

// Extra headroom per unit of measured latency jitter. On a connection whose ping
// alternates high/low, the server's estimate of our latency lags the truth by
// roughly the size of the swing, so the deviation it computes is off by that
// much in an unpredictable direction. Scale > 1 because the error can land on
// either side of the estimate.
inline constexpr float LAG_BACKTRACK_JITTER_SCALE = 2.0f;

// Floor on the usable window. Without it a badly jittering connection would
// shrink the window to nothing and disable backtrack entirely, when a shallow
// rewind is still both useful and safely inside the server's tolerance.
inline constexpr float LAG_MIN_BACKTRACK_TIME = 0.05f;

// How far past the newest network sample a render pose may sit and still be
// recorded, in ticks. With a tight interp (cl_interp 0 + cl_interp_ratio 1) the
// interpolation target sits a hair beyond the latest sample on a large fraction
// of frames, and jitter pushes it further; dropping those frames starved the
// ring, which is the difference between "backtrack is shallow" and "there are no
// records to backtrack to at all". One tick of extrapolation is still within a
// tick the server has history for. Beyond that the pose is invented, not late.
inline constexpr float LAG_MAX_EXTRAPOLATION_TICKS = 1.0f;

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
	// Reference point a record's age is measured against: the CURRENT frame's
	// pose time, curtime - GetLerp() - the same clock and the same expression
	// LagRecord_t::SimulationTime is captured on. Subtracting two samples of it
	// yields elapsed client time, which is exactly the rewind a shot asks the
	// server for and therefore exactly what the server's tolerance bounds.
	//
	// This used to hold the target's m_flSimulationTime: a server-authored stamp
	// that only advances when a snapshot for that player arrives. Its difference
	// against a client render time is not an elapsed time at all - it was off by
	// (lerp - (curtime - m_flSimulationTime)), a term that moves with ping, with
	// interp settings, and with Misc_Ping_Reducer (which reads packets early and
	// rolls curtime back, shrinking it further). At stock cl_interp 0.1 that
	// quietly cut the reachable window roughly in half, and it jittered frame to
	// frame on an unstable connection - so a shot landed on the backtracked pose
	// sometimes and not others with nothing else changed.
	//
	// Seeded to -1 to mark "no snapshot has been built for this player yet".
	float PoseReferenceTime = -1.0f;
	// Per-frame ceiling on the age of a usable record, computed once in
	// UpdateRecords so every consumer and the ghost renderer share one verdict.
	// Defaults to 0 so an unpopulated snapshot rejects every record rather than
	// accepting all of them.
	float MaxBacktrackTime = 0.0f;
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

	// Exponentially-smoothed magnitude of the frame-to-frame latency swing, i.e.
	// how unstable the connection currently is. Feeds the safety margin held back
	// from the server's tolerance in UpdateRecords: the server's own latency
	// estimate lags a swinging ping, so the deviation it computes for our tick is
	// wrong by roughly the size of the swing. Seeded to -1 like the average above
	// and cleared on the same full-ring resets.
	float m_flLatencyJitter = -1.0f;

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
	//
	// The lerp term looks like it cancels the one subtracted at capture, and
	// algebraically it does: this reduces to TIME_TO_TICKS(curtime at capture).
	// That is correct and not a no-op. The server derives its rewind target as
	// targettick = cmd->tick_count - lerpTicks, so handing it the client tick of
	// the capture frame makes it rewind to (capture curtime - lerp) - exactly the
	// render pose whose bones this record holds. Vanilla achieves the same thing
	// for the CURRENT frame by sending gpGlobals->tickcount; the rewind we gain
	// over vanilla is the client time elapsed since the record was captured, which
	// is also precisely what GetRecordAge measures and what the server bounds.
	static int GetCommandTick(float flPoseTime)
	{
		return TIME_TO_TICKS(flPoseTime + SDKUtils::GetLerp());
	}

	// Seconds of extrapolation past a player's newest network sample that a render
	// pose may carry and still be worth recording. Derived from the tick interval
	// so it tracks 100-tick servers rather than assuming 66.
	static float GetMaxExtrapolationTime()
	{
		// 1 ms floor keeps the original tolerance if interval_per_tick is not
		// readable yet, so the gate never degenerates to "reject everything".
		if (!I::GlobalVars)
			return 0.001f;

		const float flAllowed = I::GlobalVars->interval_per_tick * LAG_MAX_EXTRAPOLATION_TICKS;
		return flAllowed > 0.001f ? flAllowed : 0.001f;
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
	// when it is non-null, is not a post-teleport discontinuity, is still inside
	// the window the server will honour (cached.MaxBacktrackTime, measured
	// against cached.PoseReferenceTime), and its pose actually differs from the
	// player's live cached state (so we never backtrack onto the interpolated
	// present). Centralized so the call sites cannot drift apart as the filter
	// evolves - in particular, anything that renders a record as a "you can hit
	// this" indicator must agree with what a shot would accept.
	static bool IsRecordUsable(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached);

	// How far back a shot stamped with this record would ask the server to rewind,
	// in seconds. Both terms are client-clock pose times (see
	// LagRecordCachedState_t::PoseReferenceTime), so the difference is elapsed
	// client time - and that is exactly the quantity the server bounds: writing
	// tick_count for a record makes the deviation it computes deviate from its own
	// latency estimate by the time elapsed since that record was captured.
	//
	// Deliberately pure arithmetic over two floats, with no I::GlobalVars or
	// convar reads, so it stays executable headless in the unit tests.
	//
	// Negative or unmeasurable ages collapse to 0; a missing snapshot is rejected
	// by MaxBacktrackTime defaulting to 0 rather than by inflating the age here.
	static float GetRecordAge(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached);

	// True when a record is inside the window the server will honour. Exposed
	// separately from IsRecordUsable for consumers that need the age verdict
	// without the pose-difference test.
	static bool IsWithinBacktrackWindow(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached);

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
