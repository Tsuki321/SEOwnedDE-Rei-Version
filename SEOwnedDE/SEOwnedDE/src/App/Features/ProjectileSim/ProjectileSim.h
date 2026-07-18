#pragma once

#include "../../../SDK/SDK.h"

struct ProjectileInfo
{
	ProjectileType_t m_type{};

	Vec3 m_pos{};
	Vec3 m_ang{};

	float m_speed{};
	float m_gravity_mod{};

	bool no_spin{};
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

class CProjectileSim
{
	IPhysicsEnvironment* m_pEnv = nullptr;
	IPhysicsObject* m_pObj = nullptr;
	CPhysCollide* m_pCollide = nullptr;
	AnalyticalState m_Analytical{};
	bool m_bUseVPhysics = true;

public:
	CProjectileSim() = default;
	CProjectileSim(const CProjectileSim &) = delete;
	CProjectileSim &operator=(const CProjectileSim &) = delete;
	~CProjectileSim();
	bool GetInfo(C_TFPlayer *player, C_TFWeaponBase *weapon, const Vec3 &angles, ProjectileInfo &out);
	bool Init(const ProjectileInfo &info, bool no_vec_up = false);
	void RunTick();
	Vec3 GetOrigin();
};

MAKE_SINGLETON_SCOPED(CProjectileSim, ProjectileSim, F);
