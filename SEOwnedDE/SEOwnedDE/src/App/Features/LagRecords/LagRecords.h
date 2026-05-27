#pragma once

#include "../../../SDK/SDK.h"

#include <array>
#include <unordered_set>

inline constexpr int MAX_BONE_COUNT = 128;
inline constexpr int MAX_LAG_RECORDS = 32;
inline constexpr int MAX_ANIM_OVERLAYS = 15;

inline constexpr float LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR = 64.0f * 64.0f;

struct LayerRecord_t
{
	int m_nSequence = 0;
	float m_flCycle = 0.0f;
	float m_flWeight = 0.0f;
	int m_nOrder = 0;
};

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
	int MasterSequence = 0;
	float MasterCycle = 0.0f;
	LayerRecord_t LayerRecords[MAX_ANIM_OVERLAYS] = {};
	bool bTeleported = false;
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
	std::array<std::deque<LagRecord_t>, MAX_PLAYERS> m_LagRecords = {};
	bool m_bSettingUpBones = false;

	std::unordered_set<C_BaseEntity*> m_FailedChildBones = {};

	bool IsSimulationTimeValid(float flCurSimTime, float flCmprSimTime);

	static int PlayerToIndex(C_TFPlayer* pPlayer);

public:
	static float GetOutgoingLatency();
	void AddRecord(C_TFPlayer* pPlayer);
	const LagRecord_t* GetRecord(C_TFPlayer* pPlayer, int nRecord);
	bool HasRecords(C_TFPlayer* pPlayer, int* pTotalRecords = nullptr);
	void UpdateRecords();
	bool DiffersFromCurrent(const LagRecord_t* pRecord);
	static bool DiffersFromCurrentCached(const LagRecord_t* pRecord, const LagRecordCachedState_t& cached);
	static LagRecordCachedState_t CacheCurrentState(C_TFPlayer* pPlayer);
	const LagRecord_t* FindInterpolatedRecord(C_TFPlayer* pPlayer, float flTargetTime, LagRecord_t& outRecord);
	bool IsSettingUpBones() { return m_bSettingUpBones; }

	bool HasFailedBones(C_BaseEntity* pEntity) const
	{
		return m_FailedChildBones.contains(pEntity);
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
