#include "ProjectileSim.h"

// Leaked CTFWeaponBaseGun::FirePipeBomb sets the angular impulse to
// AngularImpulse(600, random->RandomInt(-1200, 1200), 0) - pitch is fixed at 600,
// yaw is uniform in [-1200, 1200], roll is 0. The drag basis values used by this
// sim were dumped from a server firing with 0 0 0 angles (i.e. zero spin), so we
// use 0 for the yaw here to keep the sim self-consistent. The 600/0/0 form is
// deterministic across calls; the actual game's random yaw doesn't significantly
// affect the linear trajectory through the engine's drag model.
static const Vec3 kPipeAngularVelocity{ 600.0f, 0.0f, 0.0f };

void CProjectileSim::CleanUp()
{
	if (m_pObj && m_pEnv)
	{
		m_pEnv->DestroyObject(m_pObj);
	}
	m_pObj = nullptr;

	if (m_pCollide && I::PhysicsCollision)
	{
		I::PhysicsCollision->DestroyCollide(m_pCollide);
	}
	m_pCollide = nullptr;

	if (m_pEnv && I::Physics)
	{
		I::Physics->DestroyEnvironment(m_pEnv);
	}
	m_pEnv = nullptr;
}

bool CProjectileSim::GetInfo(C_TFPlayer *player, C_TFWeaponBase *weapon, const Vec3 &angles, ProjectileInfo &out)
{
	if (!player || !weapon)
	{
		return false;
	}

	const float cur_time{ static_cast<float>(player->m_nTickBase()) * TICK_INTERVAL };
	const bool ducking{ (player->m_fFlags() & FL_DUCKING) != 0 };

	Vec3 pos{};
	Vec3 ang{};
	auto setup = [&](const Vec3 &offset, bool pipes)
	{
		SDKUtils::GetProjectileFireSetupRebuilt(player, offset, angles, pos, ang, pipes);
	};
	auto setup_standard = [&]()
	{
		setup({ 23.5f, 12.0f, ducking ? 8.0f : -3.0f }, false);
	};

	switch (weapon->GetWeaponID())
	{
		case TF_WEAPON_ROCKETLAUNCHER:
		case TF_WEAPON_PARTICLE_CANNON:
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
		{
			if (weapon->m_iItemDefinitionIndex() == Soldier_m_TheOriginal)
			{
				pos = player->GetShootPos();
				ang = angles;
			}
			else
			{
				setup_standard();
			}

			float speed{ SDKUtils::AttribHookValue(1100.0f, "mult_projectile_speed", weapon) };
			if (const int rocket_specialist{ static_cast<int>(SDKUtils::AttribHookValue(0.0f, "rocket_specialist", player)) })
			{
				speed *= Math::RemapValClamped(static_cast<float>(rocket_specialist), 1.0f, 4.0f, 1.15f, 1.6f);
				speed = std::min(speed, 3000.0f);
			}

			out = { TF_PROJECTILE_ROCKET, pos, ang, speed, 0.0f, true };
			return true;
		}

		case TF_WEAPON_GRENADELAUNCHER:
		{
			setup({ 16.0f, 8.0f, -6.0f }, true);

			auto is_lochnload{ weapon->m_iItemDefinitionIndex() == Demoman_m_TheLochnLoad };
			auto speed{ SDKUtils::AttribHookValue(1200.0, "mult_projectile_speed", weapon) };

			out = { TF_PROJECTILE_PIPEBOMB, pos, ang, speed, 1.0f, is_lochnload };

			return true;
		}

		case TF_WEAPON_PIPEBOMBLAUNCHER:
		{
			setup({ 16.0f, 8.0f, -6.0f }, true);

			auto charge_begin_time{ weapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime() };
			auto charge{ cur_time - charge_begin_time };
			auto speed{ Math::RemapValClamped(charge, 0.0f, SDKUtils::AttribHookValue(4.0f, "stickybomb_charge_rate", weapon), 900.0f, 2400.0f) };

			if (charge_begin_time <= 0.0f)
			{
				speed = 900.0f;
			}

			out = { TF_PROJECTILE_PIPEBOMB_REMOTE, pos, ang, speed, 1.0f, false };

			return true;
		}

		case TF_WEAPON_CANNON:
		{
			setup({ 16.0f, 8.0f, -6.0f }, true);

			out = { TF_PROJECTILE_CANNONBALL, pos, ang, 1454.0f, 1.0f, false };

			return true;
		}

		case TF_WEAPON_FLAREGUN:
		{
			setup_standard();

			out = { TF_PROJECTILE_FLARE, pos, ang, 2000.0f, 0.3f, true };

			return true;
		}

		case TF_WEAPON_FLAREGUN_REVENGE:
		{
			setup_standard();

			out = { TF_PROJECTILE_FLARE, pos, ang, 3000.0f, 0.45f, true };

			return true;
		}

		case TF_WEAPON_COMPOUND_BOW:
		{
			setup({ 23.5f, -8.0f, -3.0f }, false);

			auto charge_begin_time{ weapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime() };
			auto charge{ cur_time - charge_begin_time };
			auto speed{ Math::RemapValClamped(charge, 0.0f, 1.0f, 1800.0f, 2600.0f) };
			auto grav_mod{ Math::RemapValClamped(charge, 0.0f, 1.0f, 0.5f, 0.1f) };

			if (charge_begin_time <= 0.0f)
			{
				speed = 1800.0f;
				grav_mod = 0.5f;
			}

			out = { TF_PROJECTILE_ARROW, pos, ang, speed, grav_mod, true };

			return true;
		}

		case TF_WEAPON_CROSSBOW:
		case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
		{
			setup({ 23.5f, -8.0f, -3.0f }, false);

			out = { TF_PROJECTILE_ARROW, pos, ang, 2400.0f, 0.2f, true };

			return true;
		}

		case TF_WEAPON_SYRINGEGUN_MEDIC:
		{
			setup({ 16.0f, 6.0f, -8.0f }, false);

			out = { TF_PROJECTILE_SYRINGE, pos, ang, 1000.0f, 0.3f, true };

			return true;
		}

		case TF_WEAPON_FLAME_BALL:
		{
			setup_standard();
			out = { TF_PROJECTILE_FLAME_ROCKET, pos, ang, 3000.0f, 0.0f, true };
			return true;
		}

		case TF_WEAPON_FLAMETHROWER:
		{
			setup_standard();
			out = { TF_PROJECTILE_FLAME_ROCKET, pos, ang, 2000.0f, 0.0f, true };
			return true;
		}

		case TF_WEAPON_RAYGUN:
		case TF_WEAPON_DRG_POMSON:
		{
			setup_standard();
			out = {
				weapon->GetWeaponID() == TF_WEAPON_RAYGUN ? TF_PROJECTILE_ENERGY_BALL : TF_PROJECTILE_ENERGY_RING,
				pos, ang, 1200.0f, 0.0f, true
			};
			return true;
		}

		default:
		{
			return false;
		}
	}
}

bool CProjectileSim::Init(const ProjectileInfo &info, bool no_vec_up)
{
	m_bUseVPhysics = IsVPhysicsProjectile(info.m_type);

	if (m_bUseVPhysics)
	{
		if (!I::Physics || !I::PhysicsCollision)
			return false;

		if (m_pObj || m_pEnv)
			CleanUp();

		if (!m_pEnv)
		{
			m_pEnv = I::Physics->CreateEnvironment();
		}

		if (!m_pEnv)
		{
			return false;
		}

		if (!m_pObj)
		{
			if (!m_pCollide)
			{
				m_pCollide = I::PhysicsCollision->BBoxToCollide({ -2.0f, -2.0f, -2.0f }, { 2.0f, 2.0f, 2.0f });
			}

			if (!m_pCollide)
			{
				return false;
			}

			auto params{ g_PhysDefaultObjectParams };

			params.damping = 0.0f;
			params.rotdamping = 0.0f;
			params.inertia = 0.0f;
			params.rotInertiaLimit = 0.0f;
			params.enableCollisions = false;

			m_pObj = m_pEnv->CreatePolyObject(m_pCollide, 0, info.m_pos, info.m_ang, &params);

			if (m_pObj)
			{
				m_pObj->Wake();
			}
		}

		if (!m_pEnv || !m_pObj)
		{
			return false;
		}

		Vec3 forward{}, up{};

		Math::AngleVectors(info.m_ang, &forward, nullptr, &up);

		Vec3 vel{ forward * info.m_speed };
		Vec3 ang_vel{};

		if (!no_vec_up)
		{
			vel += up * 200.0f;
		}

		ang_vel = kPipeAngularVelocity;

		if (info.no_spin)
		{
			ang_vel.Zero();
		}

		m_pObj->SetPosition(info.m_pos, info.m_ang, true);
		m_pObj->SetVelocity(&vel, &ang_vel);

		float drag{ 1.0f };
		Vec3 drag_basis{};
		Vec3 ang_drag_basis{};

		switch (info.m_type)
		{
			case TF_PROJECTILE_PIPEBOMB:
			{
				drag_basis = { 0.003902f, 0.009962f, 0.009962f };
				ang_drag_basis = { 0.003618f, 0.001514f, 0.001514f };
				break;
			}

			case TF_PROJECTILE_PIPEBOMB_REMOTE:
			case TF_PROJECTILE_PIPEBOMB_PRACTICE:
			{
				drag_basis = { 0.007491f, 0.007491f, 0.007306f };
				ang_drag_basis = { 0.002777f, 0.002842f, 0.002812f };
				break;
			}

			case TF_PROJECTILE_CANNONBALL:
			{
				drag_basis = { 0.020971f, 0.019420f, 0.020971f };
				ang_drag_basis = { 0.012997f, 0.013496f, 0.013714f };
				break;
			}

			default: break;
		}

		m_pObj->SetDragCoefficient(&drag, &drag);
		m_pObj->m_dragBasis = drag_basis;
		m_pObj->m_angDragBasis = ang_drag_basis;

		physics_performanceparams_t params{};
		params.Defaults();
		params.maxVelocity = k_flMaxVelocity;
		params.maxAngularVelocity = k_flMaxAngularVelocity;

		m_pEnv->SetPerformanceSettings(&params);
		m_pEnv->SetAirDensity(2.0f);
		// Use live sv_gravity when available; GetGravity() returns 0.0f if the cvar isn't found, so fall back to the TF2 default of 800.
		const float flGravity{ SDKUtils::GetGravity() };
		m_pEnv->SetGravity({ 0.0f, 0.0f, -((flGravity > 0.0f ? flGravity : 800.0f) * info.m_gravity_mod) });
		m_pEnv->ResetSimulationClock();
	}
	else
	{
		Vec3 forward{};

		Math::AngleVectors(info.m_ang, &forward, nullptr, nullptr);

		m_Analytical.m_vecOrigin = info.m_pos;
		m_Analytical.m_vecVelocity = forward * info.m_speed;
		const float flGravity{ SDKUtils::GetGravity() };
		m_Analytical.m_flGravity = (flGravity > 0.0f ? flGravity : 800.0f) * info.m_gravity_mod;
	}

	return true;
}

void CProjectileSim::RunTick()
{
	if (m_bUseVPhysics)
	{
		if (!m_pEnv)
		{
			return;
		}

		m_pEnv->Simulate(TICK_INTERVAL);
	}
	else
	{
		const float dt = TICK_INTERVAL;
		const float grav = m_Analytical.m_flGravity;

		if (grav > 0.0f)
		{
			float newZVel = m_Analytical.m_vecVelocity.z - grav * dt;

			m_Analytical.m_vecOrigin.x += m_Analytical.m_vecVelocity.x * dt;
			m_Analytical.m_vecOrigin.y += m_Analytical.m_vecVelocity.y * dt;
			m_Analytical.m_vecOrigin.z += ((m_Analytical.m_vecVelocity.z + newZVel) * 0.5f) * dt;

			m_Analytical.m_vecVelocity.z = newZVel;
		}
		else
		{
			m_Analytical.m_vecOrigin += m_Analytical.m_vecVelocity * dt;
		}
	}
}

Vec3 CProjectileSim::GetOrigin()
{
	if (m_bUseVPhysics)
	{
		if (!m_pObj)
		{
			return {};
		}

		Vec3 out{};

		m_pObj->GetPosition(&out, nullptr);

		return out;
	}

	return m_Analytical.m_vecOrigin;
}
