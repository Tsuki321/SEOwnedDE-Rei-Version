#pragma once
#include "../AimbotCommon/AimbotCommon.h"
#include "../../LagRecords/LagRecords.h"

class CAimbotHitscan
{
	struct HitscanTarget_t : AimTarget_t
	{
		int AimedHitbox = -1;
		float SimulationTime = -1.0f;
		const LagRecord_t* LagRecord = nullptr;
		bool WasMultiPointed = false;
	};

	std::vector<HitscanTarget_t> m_vecTargets = {};
	float m_flDelayFireEndTime = 0.0f;

	// Smooth aim keeps the same entity while it remains valid and filters the
	// requested angular step so record/animation updates cannot jerk the view.
	int m_nSmoothTargetIndex = -1;
	Vec3 m_vSmoothAimStep = {};
	// Detect commands skipped while another weapon or CreateMove path was active.
	int m_nLastSmoothCommandNumber = -1;

	// Entindex of the player we last fired at, or -1 when nothing is recorded.
	// Entindexes are recycled, so 0 and -1 never count as a valid previous target.
	int m_nLastFiredTargetIndex = -1;
	// Until this time, acquiring a *different* target than the one above is held off.
	float m_flTargetSwitchEndTime = 0.0f;

	int GetAimHitbox(C_TFWeaponBase* pWeapon);
	bool ScanHead(C_TFPlayer* pLocal, HitscanTarget_t& target, const Vec3& vLocalAngles, float flFOVLimit);
	bool ScanBody(C_TFPlayer* pLocal, HitscanTarget_t& target, const Vec3& vLocalAngles, float flFOVLimit);
	bool ScanBuilding(C_TFPlayer* pLocal, HitscanTarget_t& target, const Vec3& vLocalAngles, float flFOVLimit);
	bool ValidateTarget(C_TFPlayer* pLocal, HitscanTarget_t& target, const Vec3& vLocalPos, const Vec3& vLocalAngles, float flFOVLimit);
	bool GetTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, HitscanTarget_t& outTarget);
	bool ShouldAim(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void Aim(CUserCmd* pCmd, C_TFPlayer* pLocal, const Vec3& vAngles);
	bool ShouldFire(const CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const HitscanTarget_t& target);
	void HandleFire(CUserCmd* pCmd, C_TFWeaponBase* pWeapon);
	void ResetSmoothMotion() { m_vSmoothAimStep = {}; }
	void ResetSmoothState()
	{
		m_nSmoothTargetIndex = -1;
		ResetSmoothMotion();
		m_nLastSmoothCommandNumber = -1;
	}

public:
	bool IsFiring(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon);

	// Stamp a hand-aimed shot's tick_count from the pose the crosshair ray
	// actually intersects. The live pose wins when it lies on the ray - the
	// crosshair is on it, so rewinding past it to a stale (or foreign) record
	// would move the real target off the shot server-side. Only when no live
	// hitbox is hit does a historical record claim the command. A live hit is
	// stamped via GetCommandTick(simTime) when Accuracy Improvements pins live
	// bones to the newest network pose; otherwise vanilla tick_count is left
	// alone for the interpolated present.
	// Public because CAimbot::Run drives it from OUTSIDE RunMain: every early
	// return in RunMain and in Run below used to swallow it, which is why
	// manual backtracking only worked in some game states. Sets
	// G::bCommandTickResolved on success.
	bool ResolveManualShot(CUserCmd* pCmd, C_TFPlayer* pLocal);

	void Run(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void Reset()
	{
		m_flDelayFireEndTime = 0.0f;
		m_nLastFiredTargetIndex = -1;
		m_flTargetSwitchEndTime = 0.0f;
		ResetSmoothState();
	}
};

MAKE_SINGLETON_SCOPED(CAimbotHitscan, AimbotHitscan, F);
