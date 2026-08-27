#pragma once
#include "../AimbotCommon/AimbotCommon.h"
#include "../../LagRecords/LagRecords.h"

class CAimbotMelee
{
	struct MeleeTarget_t : AimTarget_t
	{
		float SimulationTime = -1.0f;
		const LagRecord_t* LagRecord = nullptr;
		bool MeleeTraceHit = false;
	};

	std::vector<MeleeTarget_t> m_vecTargets = {};
	C_TFWeaponBase* m_pManualSwingWeapon = nullptr;
	float m_flManualSwingExpireTime = -1.0f;
	bool m_bManualSwingPending = false;
	bool m_bManualSwingImpactCommand = false;

	bool CanSee(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, MeleeTarget_t& target);
	bool GetTarget(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, MeleeTarget_t& outTarget);
	bool ShouldAim(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon);
	void Aim(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon, const Vec3& vAngles);
	bool ShouldFire(const MeleeTarget_t& target);
	void HandleFire(CUserCmd* pCmd, C_TFWeaponBase* pWeapon);

public:
	bool IsFiring(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon);
	// Captures a user-started swing before RunMain can synthesize IN_ATTACK.
	// Non-knife weapons may report the actual smack on a later command, so the
	// ownership is carried for one bounded swing window and only released when
	// the actual smack-time impact command arrives.
	bool CaptureManualSwingCommand(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon);
	bool IsManualSwingImpactCommand() const { return m_bManualSwingImpactCommand; }
	void FinishManualSwingCommand(bool bResolved);
	void ResetManualSwingState();
	// Resolves a hand-aimed swing against the pose actually intersected by the
	// finalized command's real melee hull. Historical records always win when
	// one lies on the hull. If none do, Accuracy Improvements pins live bones
	// to the newest network pose, so the live hit is stamped via
	// GetCommandTick(simTime); otherwise vanilla tick_count is left alone.
	bool ResolveManualSwing(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void Run(CUserCmd* pCmd, C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
};

MAKE_SINGLETON_SCOPED(CAimbotMelee, AimbotMelee, F);
