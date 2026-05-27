#pragma once

#include "../../../SDK/SDK.h"

#include <unordered_set>

inline constexpr int MAX_BONE_COUNT = 128;
inline constexpr int MAX_LAG_RECORDS = 128;

struct LagRecord_t
{
	C_TFPlayer* Player = nullptr;
	matrix3x4_t BoneMatrix[MAX_BONE_COUNT] = {};
	float SimulationTime = -1.0f;
	Vec3 AbsOrigin = {};
	Vec3 AbsAngles = {};
	Vec3 EyeAngles = {};
	Vec3 Velocity = {};
	Vec3 Center = {};
	int Flags = 0;
	float FeetYaw = 0.0f;
};

class CLagRecords
{
	std::unordered_map<C_TFPlayer*, std::deque<LagRecord_t>> m_LagRecords = {};
	bool m_bSettingUpBones = false;

	// Phase 2: tracks wearable / move-child entities whose SetupBones call failed
	// during the most recent AddRecord invocation. The SetupBones cache short-circuit
	// must defer to the engine implementation for these entities so a one-frame
	// stale pose is rebuilt instead of being copied from a partial cache.
	std::unordered_set<C_BaseEntity*> m_FailedChildBones = {};

	bool IsSimulationTimeValid(float flCurSimTime, float flCmprSimTime);

public:
	void AddRecord(C_TFPlayer* pPlayer);
	const LagRecord_t* GetRecord(C_TFPlayer* pPlayer, int nRecord);
	bool HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords = nullptr);
	void UpdateRecords();
	bool DiffersFromCurrent(const LagRecord_t* pRecord);
	bool IsSettingUpBones() { return m_bSettingUpBones; }

	// Phase 2: query whether a wearable / move-child entity failed its most recent
	// SetupBones capture and should bypass the cached-bone short-circuit.
	bool HasFailedBones(C_BaseEntity* pEntity) const
	{
		return m_FailedChildBones.contains(pEntity);
	}
};

MAKE_SINGLETON_SCOPED(CLagRecords, LagRecords, F);

class CLagRecordMatrixHelper
{
	// Per-player stack to support nested/overlapping CLagRecordScope usage.
	// Each entry stores the live bones/origin/angles that were active before
	// the corresponding Set() call, so Restore() pops the correct state.
	struct StackEntry_t
	{
		C_TFPlayer* Player = nullptr;
		Vec3 AbsOrigin = {};
		Vec3 AbsAngles = {};
		matrix3x4_t BoneMatrix[MAX_BONE_COUNT] = {};
		int BoneCount = 0;
	};

	std::vector<StackEntry_t> m_Stack;
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
