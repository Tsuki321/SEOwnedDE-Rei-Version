#pragma once
#include "../AimbotCommon/AimbotCommon.h"
#include "../../ProjectileSim/ProjectileSim.h"

struct ProjectileInfo;

class CAimbotProjectile
{
	struct PredictedTargetState_t
	{
		Vec3 Origin = {};
		Vec3 Mins = {};
		Vec3 Maxs = {};
		Vec3 HeadOffset = {};
		float ModelScale = 1.0f;
		bool Ducked = false;
		bool OnGround = false;
	};

	struct ProjTarget_t : AimTarget_t
	{
		float TimeToTarget = 0.0f;
		float PlannedSpeed = 0.0f;       // > 0: charge-based planned shot (bow/sticky)
		float PlannedGravityMod = 0.0f;
		float RequiredChargeTime = 0.0f; // seconds of charge to hold before releasing
	};

	struct ProjectileCandidate_t
	{
		std::vector<Vec3>* Path = nullptr;
		ProjectileInfo Launch = {};
		Vec3 Angle = {};
		float Speed = 0.0f;
		float GravityMod = 0.0f;
		float FlightTime = 0.0f;
		float TimingBias = 0.0f;
		float ChargeTime = 0.0f;
		float WaitTime = 0.0f;
		float Score = 0.0f;
		float ArcPenalty = 0.0f; // preserve the requested arc when a viable shot exists
		bool HighArc = false;
	};

	struct PredictedHitbox_t
	{
		matrix3x4_t Matrix = {};
		Vec3 Mins = {};
		Vec3 Maxs = {};
		int Index = -1;
	};

	std::vector<ProjTarget_t> m_vecTargets = {};
	std::vector<Vec3> m_TargetPath = {};
	std::vector<PredictedTargetState_t> m_TargetStates = {};
	std::vector<PredictedHitbox_t> m_TargetHitboxes = {};
	ProjectileTrajectory::Budget m_TrajectoryBudget = {};
	int m_LastAimPos = 0; // 0 = feet, 1 = body, 2 = head

	struct ChargeHoldState_t
	{
		CHandle<C_TFWeaponBase> Weapon = {};
		CHandle<C_BaseEntity> Target = {};
		Vec3 LastSolvedAngle = {};
		float RequiredChargeTime = 0.0f;
		int LastSolvedCommandNumber = -1;
		bool Active = false;
		bool ChargeObserved = false;
		bool ReleasePending = false;
		bool AbortRelease = false;
	};

	ChargeHoldState_t m_ChargeHold = {};

	struct ProjectileInfo_t
	{
		float Speed = 0.0f;
		float GravityMod = 0.0f;
		bool Pipes = false;
		bool Flamethrower = false;
	};

	ProjectileInfo_t m_CurProjInfo = {};

	float m_flDelayFireEndTime = 0.0f;

	// Entindex of the target we last fired at, or -1 when nothing is recorded.
	// Entindexes are recycled, so 0 and -1 never count as a valid previous target.
	int m_nLastFiredTargetIndex = -1;
	// Until this time, acquiring a *different* target than the one above is held off.
	float m_flTargetSwitchEndTime = 0.0f;

	bool GetProjectileInfo(C_TFWeaponBase* pWeapon);
	bool SolveProjectile(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vTo, float flSpeed, float flGravityMod,
		Vec3& vViewAngleOut, float& flTimeOut, ProjectileInfo& launchOut);
	bool SeedCandidate(C_TFPlayer* local, C_TFWeaponBase* weapon, ProjectileCandidate_t& candidate);
	bool RefineCandidate(C_TFPlayer* local, C_TFWeaponBase* weapon, ProjectileCandidate_t& candidate);
	void CaptureHitboxes(C_TFPlayer* player);
	bool SweepHitboxes(const Vec3& start, const Vec3& end, const Vec3& originStart, const Vec3& originEnd,
		float& fraction, int& hitbox) const;
	PredictedTargetState_t CaptureTargetState(C_TFPlayer* pPlayer, const Vec3& vOrigin,
		float flModelScale, const Vec3& vHeadOffset) const;
	PredictedTargetState_t GetTargetStateAtTime(float flTime) const;
	Vec3 GetAimPoint(C_TFWeaponBase* pWeapon, const PredictedTargetState_t& state, int aimPosition);
	bool CanArcReach(const ProjectileInfo& launch, float flTargetTime,
		const PredictedTargetState_t& targetState, C_BaseEntity* pTarget, float timingBias, bool requireHead,
		float* impactTime);
	bool CanSee(const ProjectileInfo& launch, const PredictedTargetState_t& targetState,
		C_BaseEntity* pTarget, float flTargetTime, float timingBias = 0.0f, bool requireHead = false,
		float* impactTime = nullptr);
	bool SolveTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd, ProjTarget_t& target);

	bool RunSplash(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd, const Vec3& vLocalPos, const Vec3& center, ProjTarget_t& target);
	bool GetTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const CUserCmd* pCmd, ProjTarget_t& outTarget);
	bool ShouldAim(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void Aim(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vAngles);
	bool ShouldFire(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void HandleFire(CUserCmd* pCmd, C_TFWeaponBase* pWeapon, C_TFPlayer* pLocal, const ProjTarget_t& target);
	void ResetChargeHold();
	bool MaintainChargeHold(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void QueueChargeRelease(CUserCmd* pCmd, bool bAbortRelease = false);

public:
	void RunChargeLifecycle(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void FinalizeChargeCommand(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, bool bConsume = true);
	bool IsFiring(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void Run(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void Reset()
	{
		ResetChargeHold();
		m_vecTargets.clear();
		m_TargetPath.clear();
		m_TargetStates.clear();
		m_TargetHitboxes.clear();
		m_LastAimPos = 0;
		m_CurProjInfo = {};
		m_flDelayFireEndTime = 0.0f;
		m_nLastFiredTargetIndex = -1;
		m_flTargetSwitchEndTime = 0.0f;
	}
};

MAKE_SINGLETON_SCOPED(CAimbotProjectile, AimbotProjectile, F);
