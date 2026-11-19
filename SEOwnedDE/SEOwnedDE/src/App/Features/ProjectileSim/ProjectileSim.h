#pragma once

#include "../../../SDK/SDK.h"
#include "ProjectileTrajectory.h"

struct ProjectileProfile
{
	const char* CollisionModel = nullptr;
	Vec3 HullMins = { -6.0f, -6.0f, -6.0f };
	Vec3 HullMaxs = { 6.0f, 6.0f, 6.0f };
	Vec3 AngularVelocity = {};
	Vec3 FallbackDragBasis = {};
	Vec3 FallbackAngularDragBasis = {};
	float MuzzleUp = 0.0f;
	float MaxFlightTime = std::numeric_limits<float>::infinity();
};

struct ProjectileInfo
{
	ProjectileType_t m_type{};

	Vec3 m_pos{};
	Vec3 m_ang{};

	float m_speed{};
	float m_gravity_mod{};

	bool no_spin{};
	ProjectileProfile m_profile{};
};

inline bool IsVPhysicsProjectile(ProjectileType_t type)
{
	switch (type)
	{
		case TF_PROJECTILE_PIPEBOMB:
		case TF_PROJECTILE_PIPEBOMB_REMOTE:
		case TF_PROJECTILE_PIPEBOMB_PRACTICE:
		case TF_PROJECTILE_CANNONBALL:
			return true;
		default:
			return false;
	}
}

struct AnalyticalState
{
	Vec3 m_vecOrigin{};
	Vec3 m_vecVelocity{};
	float m_flGravity{};
};

struct ProjectilePhysicsSnapshot
{
	float Mass = 0.0f;
	Vec3 Inertia = {};
	Vec3 LinearDrag = {};
	Vec3 AngularDrag = {};
	Vec3 Gravity = {};
	float AirDensity = 0.0f;
	float Timestep = 0.0f;
};

class CProjectileSim
{
	IPhysicsEnvironment* m_pEnv = nullptr;
	IPhysicsObject* m_pObj = nullptr;
	CPhysCollide* m_pCollide = nullptr;
	AnalyticalState m_Analytical{};
	bool m_bUseVPhysics = true;
	bool m_bOwnCollide = false;
	const char* m_CollisionModel = nullptr;
	objectparams_t m_ObjectParams{};
	Vec3 m_MassCenter{};
	int m_Material = 0;

	void DestroyBody();
	bool PrepareModel(const ProjectileProfile& profile);

public:
	CProjectileSim() = default;
	CProjectileSim(const CProjectileSim &) = delete;
	CProjectileSim &operator=(const CProjectileSim &) = delete;
	void CleanUp();
	bool GetInfo(C_TFPlayer *player, C_TFWeaponBase *weapon, const Vec3 &angles, ProjectileInfo &out);
	ProjectileTrajectory::ChargeProfile GetChargeProfile(C_TFWeaponBase* weapon);
	void ConfigureProfile(C_TFWeaponBase* weapon, ProjectileInfo& info);
	bool Init(const ProjectileInfo &info, bool no_vec_up = false, ProjectileTrajectory::Budget* budget = nullptr);
	void RunTick();
	Vec3 GetOrigin();
	bool Sample(const ProjectileInfo& info, float time, Vec3& endpoint, ProjectileTrajectory::Budget* budget = nullptr);
	static bool CaptureSnapshot(IPhysicsObject* object, IPhysicsEnvironment* environment, ProjectilePhysicsSnapshot& out);
};

MAKE_SINGLETON_SCOPED(CProjectileSim, ProjectileSim, F);
