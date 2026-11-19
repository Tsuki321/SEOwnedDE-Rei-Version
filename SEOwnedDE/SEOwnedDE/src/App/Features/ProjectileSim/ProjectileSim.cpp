#include "ProjectileSim.h"
#include <cstring>

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
	DestroyBody();

	if (m_bOwnCollide && m_pCollide && I::PhysicsCollision)
	{
		I::PhysicsCollision->DestroyCollide(m_pCollide);
	}
	m_pCollide = nullptr;
	m_bOwnCollide = false;
	m_CollisionModel = nullptr;

	if (m_pEnv && I::Physics)
	{
		I::Physics->DestroyEnvironment(m_pEnv);
	}
	m_pEnv = nullptr;
}

void CProjectileSim::DestroyBody()
{
	if (m_pObj && m_pEnv) m_pEnv->DestroyObject(m_pObj);
	m_pObj = nullptr;
}

ProjectileTrajectory::ChargeProfile CProjectileSim::GetChargeProfile(C_TFWeaponBase* weapon)
{
	if (!weapon) return {};
	const float multiplier = SDKUtils::AttribHookValue(1.0f, "mult_projectile_speed", weapon);
	if (weapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
		return { 1.0f, 1800.0f * multiplier, 2600.0f * multiplier, 0.5f, 0.1f };
	if (weapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
		return { SDKUtils::AttribHookValue(4.0f, "stickybomb_charge_rate", weapon),
			900.0f * multiplier, 2400.0f * multiplier, 1.0f, 1.0f };
	return {};
}

void CProjectileSim::ConfigureProfile(C_TFWeaponBase* weapon, ProjectileInfo& info)
{
	auto& profile = info.m_profile;
	profile = {};
	if (IsVPhysicsProjectile(info.m_type))
	{
		profile.HullMins = { -2.0f, -2.0f, -2.0f };
		profile.HullMaxs = { 2.0f, 2.0f, 2.0f };
		profile.MuzzleUp = 200.0f;
		profile.AngularVelocity = info.no_spin ? Vec3{} : kPipeAngularVelocity;
		profile.MaxFlightTime = info.m_type == TF_PROJECTILE_CANNONBALL ? 0.95f
			: weapon && weapon->m_iItemDefinitionIndex() == Demoman_m_TheIronBomber ? 1.4f : 2.0f;
		switch (info.m_type)
		{
			case TF_PROJECTILE_PIPEBOMB_REMOTE:
			case TF_PROJECTILE_PIPEBOMB_PRACTICE:
				profile.CollisionModel = "models/weapons/w_models/w_stickybomb.mdl";
				profile.FallbackDragBasis = { 0.007491f, 0.007491f, 0.007306f };
				profile.FallbackAngularDragBasis = { 0.002777f, 0.002842f, 0.002812f };
				break;
			case TF_PROJECTILE_CANNONBALL:
				profile.CollisionModel = "models/weapons/w_models/w_cannonball.mdl";
				profile.FallbackDragBasis = { 0.020971f, 0.019420f, 0.020971f };
				profile.FallbackAngularDragBasis = { 0.012997f, 0.013496f, 0.013714f };
				break;
			default:
				profile.CollisionModel = weapon && weapon->m_iItemDefinitionIndex() == Demoman_m_TheIronBomber
					? "models/workshop/weapons/c_models/c_quadball/w_quadball_grenade.mdl"
					: "models/weapons/w_models/w_grenade_grenadelauncher.mdl";
				profile.FallbackDragBasis = { 0.003902f, 0.009962f, 0.009962f };
				profile.FallbackAngularDragBasis = { 0.003618f, 0.001514f, 0.001514f };
				break;
		}
	}
	else if (info.m_type == TF_PROJECTILE_ROCKET)
	{
		profile.HullMins = {};
		profile.HullMaxs = {};
	}
	if (info.m_type == TF_PROJECTILE_FLARE)
	{
		profile.HullMins = { -8.0f, -8.0f, -8.0f };
		profile.HullMaxs = { 8.0f, 8.0f, 8.0f };
	}
	if (info.m_type == TF_PROJECTILE_FLAME_ROCKET)
	{
		profile.HullMins = { -12.0f, -12.0f, -12.0f };
		profile.HullMaxs = { 12.0f, 12.0f, 12.0f };
		profile.MaxFlightTime = 0.18f;
	}
}

bool CProjectileSim::PrepareModel(const ProjectileProfile& profile)
{
	if (m_pCollide && !m_bOwnCollide && m_CollisionModel == profile.CollisionModel) return true;
	if (m_bOwnCollide && m_pCollide) I::PhysicsCollision->DestroyCollide(m_pCollide);
	m_pCollide = nullptr;
	m_bOwnCollide = false;
	m_CollisionModel = profile.CollisionModel;
	m_ObjectParams = g_PhysDefaultObjectParams;
	m_Material = 0;
	if (I::ModelInfoClient && profile.CollisionModel)
	{
		const auto model = I::ModelInfoClient->FindOrLoadModel(profile.CollisionModel);
		const auto collide = model ? I::ModelInfoClient->GetVCollide(model) : nullptr;
		if (collide && collide->solidCount > 0 && collide->solids && collide->pKeyValues)
		{
			auto parser = I::PhysicsCollision->VPhysicsKeyParserCreate(collide->pKeyValues);
			if (parser)
			{
				class Defaults final : public IVPhysicsKeyHandler
				{
					void ParseKeyValue(void*, const char*, const char*) override {}
					void SetDefaults(void* data) override { static_cast<solid_t*>(data)->params = g_PhysDefaultObjectParams; }
				} defaults;
				while (!parser->Finished())
				{
					if (std::strcmp(parser->GetCurrentBlockName(), "solid") != 0)
					{
						parser->SkipBlock();
						continue;
					}
					solid_t solid{};
					parser->ParseSolid(&solid, &defaults);
					if (solid.index < 0 || solid.index >= collide->solidCount || !(solid.params.mass > 0.0f)) continue;
					m_pCollide = collide->solids[solid.index];
					m_ObjectParams = solid.params;
					m_MassCenter = solid.massCenterOverride;
					if (solid.params.massCenterOverride) m_ObjectParams.massCenterOverride = &m_MassCenter;
					break;
				}
				I::PhysicsCollision->VPhysicsKeyParserDestroy(parser);
			}
		}
	}
	if (!m_pCollide)
	{
		// Models may be unavailable during loading. Preserve a deterministic legacy
		// fallback, while the solver still refines against its actual native motion.
		m_pCollide = I::PhysicsCollision->BBoxToCollide(profile.HullMins, profile.HullMaxs);
		m_bOwnCollide = m_pCollide != nullptr;
		m_ObjectParams.damping = 0.0f;
		m_ObjectParams.rotdamping = 0.0f;
	}
	// Parser-owned string pointers must not outlive the parser.
	m_ObjectParams.pName = profile.CollisionModel;
	m_ObjectParams.pGameData = nullptr;
	m_ObjectParams.enableCollisions = false;
	return m_pCollide != nullptr;
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
		Vec3 firingOffset = offset;
		if (weapon->m_iItemDefinitionIndex() == Soldier_m_TheOriginal
			|| SDKUtils::AttribHookValue(0.0f, "centerfire_projectile", weapon) == 1.0f)
			firingOffset.y = 0.0f;
		SDKUtils::GetProjectileFireSetupRebuilt(player, firingOffset, angles, pos, ang, pipes);
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
			setup_standard();

			float speed{ SDKUtils::AttribHookValue(1100.0f, "mult_projectile_speed", weapon) };
			if (const int rocket_specialist{ static_cast<int>(SDKUtils::AttribHookValue(0.0f, "rocket_specialist", player)) })
			{
				speed *= Math::RemapValClamped(static_cast<float>(rocket_specialist), 1.0f, 4.0f, 1.15f, 1.6f);
				speed = std::min(speed, 3000.0f);
			}

			out = { TF_PROJECTILE_ROCKET, pos, ang, speed, 0.0f, true };
			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_GRENADELAUNCHER:
		{
			setup({ 16.0f, 8.0f, -6.0f }, true);

			auto is_lochnload{ weapon->m_iItemDefinitionIndex() == Demoman_m_TheLochnLoad };
			auto speed{ SDKUtils::AttribHookValue(1200.0, "mult_projectile_speed", weapon) };

			out = { TF_PROJECTILE_PIPEBOMB, pos, ang, speed, 1.0f, is_lochnload };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_PIPEBOMBLAUNCHER:
		{
			setup({ 16.0f, 8.0f, -6.0f }, true);

			auto charge_begin_time{ weapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime() };
			float speed{}, gravity{};
			GetChargeProfile(weapon).Evaluate(charge_begin_time > 0.0f ? cur_time - charge_begin_time : 0.0f, speed, gravity);

			out = { TF_PROJECTILE_PIPEBOMB_REMOTE, pos, ang, speed, 1.0f, false };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_CANNON:
		{
			setup({ 16.0f, 8.0f, -6.0f }, true);

			out = { TF_PROJECTILE_CANNONBALL, pos, ang, 1454.0f, 1.0f, false };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_FLAREGUN:
		{
			setup_standard();

			out = { TF_PROJECTILE_FLARE, pos, ang, 2000.0f, 0.3f, true };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_FLAREGUN_REVENGE:
		{
			setup_standard();

			out = { TF_PROJECTILE_FLARE, pos, ang, 3000.0f, 0.45f, true };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_COMPOUND_BOW:
		{
			setup({ 23.5f, -8.0f, -3.0f }, false);

			auto charge_begin_time{ weapon->As<C_TFPipebombLauncher>()->m_flChargeBeginTime() };
			float speed{}, grav_mod{};
			GetChargeProfile(weapon).Evaluate(charge_begin_time > 0.0f ? cur_time - charge_begin_time : 0.0f, speed, grav_mod);

			out = { TF_PROJECTILE_ARROW, pos, ang, speed, grav_mod, true };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_CROSSBOW:
		case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
		{
			setup({ 23.5f, -8.0f, -3.0f }, false);

			out = { TF_PROJECTILE_ARROW, pos, ang, 2400.0f, 0.2f, true };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_SYRINGEGUN_MEDIC:
		{
			setup({ 16.0f, 6.0f, -8.0f }, false);

			out = { TF_PROJECTILE_SYRINGE, pos, ang, 1000.0f, 0.3f, true };

			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_FLAME_BALL:
		{
			setup_standard();
			out = { TF_PROJECTILE_FLAME_ROCKET, pos, ang, 3000.0f, 0.0f, true };
			ConfigureProfile(weapon, out);
			return true;
		}

		case TF_WEAPON_FLAMETHROWER:
		{
			setup_standard();
			out = { TF_PROJECTILE_FLAME_ROCKET, pos, ang, 2000.0f, 0.0f, true };
			ConfigureProfile(weapon, out);
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
			ConfigureProfile(weapon, out);
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
	if (!ProjectileTrajectory::IsFinite(info.m_pos) || !ProjectileTrajectory::IsFinite(info.m_ang)
		|| !std::isfinite(info.m_speed) || !(info.m_speed > 0.0f)
		|| !std::isfinite(info.m_gravity_mod) || info.m_gravity_mod < 0.0f)
		return false;
	m_bUseVPhysics = IsVPhysicsProjectile(info.m_type);
	const float configuredGravity = SDKUtils::GetGravity();
	Vec3 gravity{ 0.0f, 0.0f, -(configuredGravity > 0.0f ? configuredGravity : 800.0f) };
	float airDensity = 2.0f, timestep = TICK_INTERVAL;
	physics_performanceparams_t performance = g_PhysDefaultPerformanceParams;
	const auto liveEnvironment = I::Physics ? I::Physics->GetActiveEnvironmentByIndex(0) : nullptr;
	if (liveEnvironment && liveEnvironment != m_pEnv)
	{
		liveEnvironment->GetGravity(&gravity);
		airDensity = liveEnvironment->GetAirDensity();
		timestep = liveEnvironment->GetSimulationTimestep();
		liveEnvironment->GetPerformanceSettings(&performance);
	}
	if (!ProjectileTrajectory::IsFinite(gravity) || !std::isfinite(airDensity) || airDensity < 0.0f
		|| !std::isfinite(timestep) || !(timestep > 0.0f)) return false;
	if (m_bUseVPhysics)
	{
		if (!I::Physics || !I::PhysicsCollision) return false;
		DestroyBody();
		if (!m_pEnv) m_pEnv = I::Physics->CreateEnvironment();
		if (!m_pEnv || !PrepareModel(info.m_profile)) return false;
		m_pEnv->SetPerformanceSettings(&performance);
		m_pEnv->SetAirDensity(airDensity);
		m_pEnv->SetGravity(gravity * info.m_gravity_mod);
		m_pEnv->SetSimulationTimestep(timestep);
		m_pEnv->ResetSimulationClock();
		m_pObj = m_pEnv->CreatePolyObject(m_pCollide, m_Material, info.m_pos, info.m_ang, &m_ObjectParams);
		if (!m_pObj) return false;
		if (m_bOwnCollide)
		{
			// Build 26334388: private bases at 0x28/0x34 are confined to the
			// model-unavailable fallback. Native models derive them from their body.
			m_pObj->m_dragBasis = info.m_profile.FallbackDragBasis;
			m_pObj->m_angDragBasis = info.m_profile.FallbackAngularDragBasis;
		}
		float drag = m_bOwnCollide ? 1.0f : m_ObjectParams.dragCoefficient;
		m_pObj->SetDragCoefficient(&drag, &drag);
		Vec3 forward{}, up{};
		Math::AngleVectors(info.m_ang, &forward, nullptr, &up);
		const Vec3 velocity = forward * info.m_speed + up * (no_vec_up ? 0.0f : info.m_profile.MuzzleUp);
		const Vec3 angularVelocity = info.no_spin ? Vec3{} : info.m_profile.AngularVelocity;
		m_pObj->SetVelocityInstantaneous(&velocity, &angularVelocity);
		m_pObj->Wake();
	}
	else
	{
		Vec3 forward{};
		Math::AngleVectors(info.m_ang, &forward, nullptr, nullptr);
		m_Analytical.m_vecOrigin = info.m_pos;
		m_Analytical.m_vecVelocity = forward * info.m_speed;
		m_Analytical.m_flGravity = -gravity.z * info.m_gravity_mod;
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

bool CProjectileSim::Sample(const ProjectileInfo& info, float time, Vec3& endpoint, ProjectileTrajectory::Budget* budget)
{
	if (!std::isfinite(time) || time < 0.0f || !(TICK_INTERVAL > 0.0f)
		|| (budget && !budget->TakeTrial()) || !Init(info)) return false;
	float elapsed = 0.0f;
	endpoint = GetOrigin();
	while (elapsed + 1e-6f < time)
	{
		if (budget && !budget->TakeStep()) return false;
		const Vec3 before = GetOrigin();
		RunTick();
		const float step = std::min(TICK_INTERVAL, time - elapsed);
		endpoint = before + (GetOrigin() - before) * (step / TICK_INTERVAL);
		elapsed += step;
	}
	return ProjectileTrajectory::IsFinite(endpoint);
}

bool CProjectileSim::CaptureSnapshot(IPhysicsObject* object, IPhysicsEnvironment* environment, ProjectilePhysicsSnapshot& out)
{
	if (!object || !environment) return false;
	out.Mass = object->GetMass();
	out.Inertia = object->GetInertia();
	for (int axis = 0; axis < 3; ++axis)
	{
		Vec3 direction{};
		direction[axis] = 1.0f;
		out.LinearDrag[axis] = object->CalculateLinearDrag(direction);
		out.AngularDrag[axis] = object->CalculateAngularDrag(direction);
	}
	environment->GetGravity(&out.Gravity);
	out.AirDensity = environment->GetAirDensity();
	out.Timestep = environment->GetSimulationTimestep();
	return std::isfinite(out.Mass) && out.Mass > 0.0f && ProjectileTrajectory::IsFinite(out.Inertia)
		&& ProjectileTrajectory::IsFinite(out.LinearDrag) && ProjectileTrajectory::IsFinite(out.AngularDrag);
}
