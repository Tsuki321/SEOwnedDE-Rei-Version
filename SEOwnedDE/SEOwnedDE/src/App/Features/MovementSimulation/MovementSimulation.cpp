#include "MovementSimulation.h"

#include "../LagRecords/LagRecords.h"

#include "../CFG.h"

void CMovementSimulation::CPlayerDataBackup::Store(C_TFPlayer* pPlayer)
{
	m_vecOrigin = pPlayer->m_vecOrigin();
	m_vecVelocity = pPlayer->m_vecVelocity();
	m_vecBaseVelocity = pPlayer->m_vecBaseVelocity();
	m_vecViewOffset = pPlayer->m_vecViewOffset();
	m_hGroundEntity = pPlayer->m_hGroundEntity();
	m_fFlags = pPlayer->m_fFlags();
	m_flDucktime = pPlayer->m_flDucktime();
	m_flDuckJumpTime = pPlayer->m_flDuckJumpTime();
	m_bDucked = pPlayer->m_bDucked();
	m_bDucking = pPlayer->m_bDucking();
	m_bInDuckJump = pPlayer->m_bInDuckJump();
	m_flModelScale = pPlayer->m_flModelScale();
	m_nButtons = pPlayer->m_nButtons();
	m_flLastMovementStunChange = pPlayer->m_flLastMovementStunChange();
	m_flStunLerpTarget = pPlayer->m_flStunLerpTarget();
	m_bStunNeedsFadeOut = pPlayer->m_bStunNeedsFadeOut();
	m_flPrevTauntYaw = pPlayer->m_flPrevTauntYaw();
	m_flTauntYaw = pPlayer->m_flTauntYaw();
	m_flCurrentTauntMoveSpeed = pPlayer->m_flCurrentTauntMoveSpeed();
	m_iKartState = pPlayer->m_iKartState();
	m_flVehicleReverseTime = pPlayer->m_flVehicleReverseTime();
	m_flHypeMeter = pPlayer->m_flHypeMeter();
	m_flMaxspeed = pPlayer->m_flMaxspeed();
	m_nAirDucked = pPlayer->m_nAirDucked();
	m_bJumping = pPlayer->m_bJumping();
	m_iAirDash = pPlayer->m_iAirDash();
	m_flWaterJumpTime = pPlayer->m_flWaterJumpTime();
	m_flSwimSoundTime = pPlayer->m_flSwimSoundTime();
	m_surfaceProps = pPlayer->m_surfaceProps();
	m_pSurfaceData = pPlayer->m_pSurfaceData();
	m_surfaceFriction = pPlayer->m_surfaceFriction();
	m_chTextureType = pPlayer->m_chTextureType();
	m_vecPunchAngle = pPlayer->m_vecPunchAngle();
	m_vecPunchAngleVel = pPlayer->m_vecPunchAngleVel();
	m_MoveType = pPlayer->m_MoveType();
	m_MoveCollide = pPlayer->m_MoveCollide();
	m_vecLadderNormal = pPlayer->m_vecLadderNormal();
	m_flGravity = pPlayer->m_flGravity();
	m_nWaterLevel = pPlayer->m_nWaterLevel_C_BaseEntity();
	m_nWaterType = pPlayer->m_nWaterType();
	m_flFallVelocity = pPlayer->m_flFallVelocity();
	m_flJumpTime = pPlayer->m_flJumpTime();
	m_nPlayerCond = pPlayer->m_nPlayerCond();
	m_nPlayerCondEx = pPlayer->m_nPlayerCondEx();
	m_nPlayerCondEx2 = pPlayer->m_nPlayerCondEx2();
	m_nPlayerCondEx3 = pPlayer->m_nPlayerCondEx3();
	m_nPlayerCondEx4 = pPlayer->m_nPlayerCondEx4();
	_condition_bits = pPlayer->_condition_bits();
}

void CMovementSimulation::CPlayerDataBackup::Restore(C_TFPlayer* pPlayer)
{
	pPlayer->m_vecOrigin() = m_vecOrigin;
	pPlayer->m_vecVelocity() = m_vecVelocity;
	pPlayer->m_vecBaseVelocity() = m_vecBaseVelocity;
	pPlayer->m_vecViewOffset() = m_vecViewOffset;
	pPlayer->m_hGroundEntity() = m_hGroundEntity;
	pPlayer->m_fFlags() = m_fFlags;
	pPlayer->m_flDucktime() = m_flDucktime;
	pPlayer->m_flDuckJumpTime() = m_flDuckJumpTime;
	pPlayer->m_bDucked() = m_bDucked;
	pPlayer->m_bDucking() = m_bDucking;
	pPlayer->m_bInDuckJump() = m_bInDuckJump;
	pPlayer->m_flModelScale() = m_flModelScale;
	pPlayer->m_nButtons() = m_nButtons;
	pPlayer->m_flLastMovementStunChange() = m_flLastMovementStunChange;
	pPlayer->m_flStunLerpTarget() = m_flStunLerpTarget;
	pPlayer->m_bStunNeedsFadeOut() = m_bStunNeedsFadeOut;
	pPlayer->m_flPrevTauntYaw() = m_flPrevTauntYaw;
	pPlayer->m_flTauntYaw() = m_flTauntYaw;
	pPlayer->m_flCurrentTauntMoveSpeed() = m_flCurrentTauntMoveSpeed;
	pPlayer->m_iKartState() = m_iKartState;
	pPlayer->m_flVehicleReverseTime() = m_flVehicleReverseTime;
	pPlayer->m_flHypeMeter() = m_flHypeMeter;
	pPlayer->m_flMaxspeed() = m_flMaxspeed;
	pPlayer->m_nAirDucked() = m_nAirDucked;
	pPlayer->m_bJumping() = m_bJumping;
	pPlayer->m_iAirDash() = m_iAirDash;
	pPlayer->m_flWaterJumpTime() = m_flWaterJumpTime;
	pPlayer->m_flSwimSoundTime() = m_flSwimSoundTime;
	pPlayer->m_surfaceProps() = m_surfaceProps;
	pPlayer->m_pSurfaceData() = m_pSurfaceData;
	pPlayer->m_surfaceFriction() = m_surfaceFriction;
	pPlayer->m_chTextureType() = m_chTextureType;
	pPlayer->m_vecPunchAngle() = m_vecPunchAngle;
	pPlayer->m_vecPunchAngleVel() = m_vecPunchAngleVel;
	pPlayer->m_flJumpTime() = m_flJumpTime;
	pPlayer->m_MoveType() = m_MoveType;
	pPlayer->m_MoveCollide() = m_MoveCollide;
	pPlayer->m_vecLadderNormal() = m_vecLadderNormal;
	pPlayer->m_flGravity() = m_flGravity;
	pPlayer->m_nWaterLevel_C_BaseEntity() = m_nWaterLevel;
	pPlayer->m_nWaterType() = m_nWaterType;
	pPlayer->m_flFallVelocity() = m_flFallVelocity;
	pPlayer->m_nPlayerCond() = m_nPlayerCond;
	pPlayer->m_nPlayerCondEx() = m_nPlayerCondEx;
	pPlayer->m_nPlayerCondEx2() = m_nPlayerCondEx2;
	pPlayer->m_nPlayerCondEx3() = m_nPlayerCondEx3;
	pPlayer->m_nPlayerCondEx4() = m_nPlayerCondEx4;
	pPlayer->_condition_bits() = _condition_bits;
}

void CMovementSimulation::SetupMoveData(C_TFPlayer* pPlayer, CMoveData* pMoveData)
{
	if (!pPlayer || !pMoveData)
		return;

	pMoveData->m_bFirstRunOfFunctions = false;
	pMoveData->m_bGameCodeMovedPlayer = false;
	pMoveData->m_nPlayerHandle = pPlayer->GetRefEHandle();
	pMoveData->m_vecVelocity = pPlayer->m_vecVelocity();
	pMoveData->m_vecAbsOrigin = pPlayer->m_vecOrigin();
	pMoveData->m_flMaxSpeed = pPlayer->TeamFortress_CalculateMaxSpeed();

	if (m_PlayerDataBackup.m_fFlags & FL_DUCKING)
		pMoveData->m_flMaxSpeed *= 0.3333f;

	pMoveData->m_flClientMaxSpeed = pMoveData->m_flMaxSpeed;

	pMoveData->m_vecViewAngles = {0.0f, Math::VelocityToAngles(pMoveData->m_vecVelocity).y, 0.0f};

	if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 0)
	{
		pMoveData->m_flForwardMove = 450.0f;
		pMoveData->m_flSideMove = 0.0f;
	}

	else if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 1)
	{
		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(pMoveData->m_vecViewAngles, &vForward, &vRight, nullptr);

		if (fabsf(vRight.x) > 0.001f)
		{
			const float flRatio = vRight.y / vRight.x;
			const float flDenom = vForward.y - flRatio * vForward.x;

			if (fabsf(flDenom) > 0.001f)
				pMoveData->m_flForwardMove = (pMoveData->m_vecVelocity.y - flRatio * pMoveData->m_vecVelocity.x) / flDenom;
			else
				pMoveData->m_flForwardMove = 0.0f;

			pMoveData->m_flSideMove = (pMoveData->m_vecVelocity.x - vForward.x * pMoveData->m_flForwardMove) / vRight.x;
		}
		else
		{
			pMoveData->m_flForwardMove = 450.0f;
			pMoveData->m_flSideMove = 0.0f;
		}
	}

	else if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 2)
	{
		// Velocity Extrapolation: use lag records to compute acceleration trend
		// and project a predicted velocity, then decompose into move inputs
		Vec3 vPredictedVelocity = pMoveData->m_vecVelocity;

		if (F::LagRecords->HasRecords(pPlayer))
		{
			const LagRecord_t* rec0 = F::LagRecords->GetRecord(pPlayer, 0);
			const LagRecord_t* rec1 = F::LagRecords->GetRecord(pPlayer, 1);
			const LagRecord_t* rec2 = F::LagRecords->GetRecord(pPlayer, 2);
			const LagRecord_t* rec3 = F::LagRecords->GetRecord(pPlayer, 3);

			if (rec0 && rec1 && rec2 && rec3)
			{
				// Compute acceleration between each pair of records, scaled to per-tick using actual time deltas
				const float dt01 = std::max(rec0->SimulationTime - rec1->SimulationTime, TICK_INTERVAL);
				const float dt12 = std::max(rec1->SimulationTime - rec2->SimulationTime, TICK_INTERVAL);
				const float dt23 = std::max(rec2->SimulationTime - rec3->SimulationTime, TICK_INTERVAL);

				const Vec3 accel01 = (rec0->Velocity - rec1->Velocity) / dt01 * TICK_INTERVAL;
				const Vec3 accel12 = (rec1->Velocity - rec2->Velocity) / dt12 * TICK_INTERVAL;
				const Vec3 accel23 = (rec2->Velocity - rec3->Velocity) / dt23 * TICK_INTERVAL;

				// weighted average: more recent acceleration is more important
				// weights: 0.5 for most recent, 0.3 for middle, 0.2 for oldest
				Vec3 vAvgAccel = {};
				vAvgAccel.x = accel01.x * 0.5f + accel12.x * 0.3f + accel23.x * 0.2f;
				vAvgAccel.y = accel01.y * 0.5f + accel12.y * 0.3f + accel23.y * 0.2f;
				vAvgAccel.z = 0.0f; // don't extrapolate vertical, gravity handles it

				// apply one tick of extrapolated acceleration to current velocity
				vPredictedVelocity.x += vAvgAccel.x;
				vPredictedVelocity.y += vAvgAccel.y;

				// clamp to max speed to avoid unrealistic predictions
				const float flPredSpeed = vPredictedVelocity.Length2D();
				const float flMaxSpeedLimit = pMoveData->m_flMaxSpeed * 1.2f; // allow slight overshoot for acceleration
				if (flPredSpeed > flMaxSpeedLimit && flPredSpeed > 0.001f)
				{
					const float flScale = flMaxSpeedLimit / flPredSpeed;
					vPredictedVelocity.x *= flScale;
					vPredictedVelocity.y *= flScale;
				}

				// Store for per-tick application in RunTick
				m_vMethod2Accel = vAvgAccel;
				m_vMethod2Velocity = vPredictedVelocity;
			}
			else if (rec0 && rec1)
			{
				// fallback: only 2 records available, scale by actual time delta
				const float dt = std::max(rec0->SimulationTime - rec1->SimulationTime, TICK_INTERVAL);
				const Vec3 vAccel = (rec0->Velocity - rec1->Velocity) / dt * TICK_INTERVAL;
				vPredictedVelocity.x += vAccel.x;
				vPredictedVelocity.y += vAccel.y;

				const float flPredSpeed = vPredictedVelocity.Length2D();
				const float flMaxSpeedLimit = pMoveData->m_flMaxSpeed * 1.2f;
				if (flPredSpeed > flMaxSpeedLimit && flPredSpeed > 0.001f)
				{
					const float flScale = flMaxSpeedLimit / flPredSpeed;
					vPredictedVelocity.x *= flScale;
					vPredictedVelocity.y *= flScale;
				}

				// Store fallback accel for per-tick application
				m_vMethod2Accel = vAccel;
				m_vMethod2Velocity = vPredictedVelocity;
			}
		}

		// update view angles to match predicted velocity direction
		if (vPredictedVelocity.Length2D() > 1.0f)
			pMoveData->m_vecViewAngles = { 0.0f, Math::VelocityToAngles(vPredictedVelocity).y, 0.0f };

		// decompose predicted velocity into forward/side move
		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(pMoveData->m_vecViewAngles, &vForward, &vRight, nullptr);

		if (fabsf(vRight.x) > 0.001f)
		{
			const float flRatio = vRight.y / vRight.x;
			const float flDenom = vForward.y - flRatio * vForward.x;

			if (fabsf(flDenom) > 0.001f)
				pMoveData->m_flForwardMove = (vPredictedVelocity.y - flRatio * vPredictedVelocity.x) / flDenom;
			else
				pMoveData->m_flForwardMove = 0.0f;

			pMoveData->m_flSideMove = (vPredictedVelocity.x - vForward.x * pMoveData->m_flForwardMove) / vRight.x;
		}
		else
		{
			pMoveData->m_flForwardMove = 450.0f;
			pMoveData->m_flSideMove = 0.0f;
		}
	}

	else if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 3)
	{
		// Adaptive Accel Tracking: averages recent lag-record acceleration,
		// then updates movement inputs each simulation tick with drift limiting.
		m_vAccelTrend = {};
		m_vAdaptiveVelocity = pMoveData->m_vecVelocity;

		if (F::LagRecords->HasRecords(pPlayer))
		{
			const LagRecord_t* rec0 = F::LagRecords->GetRecord(pPlayer, 0);
			const LagRecord_t* rec1 = F::LagRecords->GetRecord(pPlayer, 1);
			const LagRecord_t* rec2 = F::LagRecords->GetRecord(pPlayer, 2);
			const LagRecord_t* rec3 = F::LagRecords->GetRecord(pPlayer, 3);
			const LagRecord_t* rec4 = F::LagRecords->GetRecord(pPlayer, 4);

	if (rec0 && rec1 && rec2 && rec3 && rec4)
			{
				const float dt01 = std::max(rec0->SimulationTime - rec1->SimulationTime, TICK_INTERVAL);
				const float dt12 = std::max(rec1->SimulationTime - rec2->SimulationTime, TICK_INTERVAL);
				const float dt23 = std::max(rec2->SimulationTime - rec3->SimulationTime, TICK_INTERVAL);
				const float dt34 = std::max(rec3->SimulationTime - rec4->SimulationTime, TICK_INTERVAL);

				const Vec3 accel01 = (rec0->Velocity - rec1->Velocity) / dt01;
				const Vec3 accel12 = (rec1->Velocity - rec2->Velocity) / dt12;
				const Vec3 accel23 = (rec2->Velocity - rec3->Velocity) / dt23;
				const Vec3 accel34 = (rec3->Velocity - rec4->Velocity) / dt34;

				m_vAccelTrend.x = (accel01.x + accel12.x + accel23.x + accel34.x) * 0.25f * TICK_INTERVAL;
				m_vAccelTrend.y = (accel01.y + accel12.y + accel23.y + accel34.y) * 0.25f * TICK_INTERVAL;
				m_vAccelTrend.z = 0.0f;
				m_vAdaptiveVelocity = rec0->Velocity;
			}
			else if (rec0 && rec1)
			{
				const float dt = std::max(rec0->SimulationTime - rec1->SimulationTime, TICK_INTERVAL);
				m_vAccelTrend.x = (rec0->Velocity.x - rec1->Velocity.x) / dt * TICK_INTERVAL;
				m_vAccelTrend.y = (rec0->Velocity.y - rec1->Velocity.y) / dt * TICK_INTERVAL;
				m_vAccelTrend.z = 0.0f;
				m_vAdaptiveVelocity = rec0->Velocity;
			}

			// Store original velocity for drift limiting
			m_vMethod3OriginalVelocity = m_vAdaptiveVelocity;

			// Clamp accel trend to reasonable range (prevent over-prediction)
			const float flAccelMag = m_vAccelTrend.Length2D();
			const float flMaxAccel = pMoveData->m_flMaxSpeed * 0.15f;
			if (flAccelMag > flMaxAccel && flAccelMag > 0.001f)
			{
				m_vAccelTrend.x *= flMaxAccel / flAccelMag;
				m_vAccelTrend.y *= flMaxAccel / flAccelMag;
			}
		}

		// Initial decomposition of current velocity into movement inputs
		if (m_vAdaptiveVelocity.Length2D() > 1.0f)
			pMoveData->m_vecViewAngles = { 0.0f, Math::VelocityToAngles(m_vAdaptiveVelocity).y, 0.0f };

		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(pMoveData->m_vecViewAngles, &vForward, &vRight, nullptr);

		if (fabsf(vRight.x) > 0.001f)
		{
			const float flRatio = vRight.y / vRight.x;
			const float flDenom = vForward.y - flRatio * vForward.x;

			if (fabsf(flDenom) > 0.001f)
				pMoveData->m_flForwardMove = (m_vAdaptiveVelocity.y - flRatio * m_vAdaptiveVelocity.x) / flDenom;
			else
				pMoveData->m_flForwardMove = 0.0f;

			pMoveData->m_flSideMove = (m_vAdaptiveVelocity.x - vForward.x * pMoveData->m_flForwardMove) / vRight.x;
		}
		else
		{
			pMoveData->m_flForwardMove = 450.0f;
			pMoveData->m_flSideMove = 0.0f;
		}
	}

	const float flSpeed = pPlayer->m_vecVelocity().Length2D();

	if (flSpeed <= pMoveData->m_flMaxSpeed * 0.1f)
		pMoveData->m_flForwardMove = pMoveData->m_flSideMove = 0.0f;

	pMoveData->m_vecAngles = pMoveData->m_vecViewAngles;
	pMoveData->m_vecOldAngles = pMoveData->m_vecAngles;

	if (pPlayer->m_hConstraintEntity())
		pMoveData->m_vecConstraintCenter = pPlayer->m_hConstraintEntity()->GetAbsOrigin();

	else pMoveData->m_vecConstraintCenter = pPlayer->m_vecConstraintCenter();

	pMoveData->m_flConstraintRadius = pPlayer->m_flConstraintRadius();
	pMoveData->m_flConstraintWidth = pPlayer->m_flConstraintWidth();
	pMoveData->m_flConstraintSpeedFactor = pPlayer->m_flConstraintSpeedFactor();

	m_flYawTurnRate = 0.0f;

	// Wraps a yaw angle difference to [-180, 180] to handle Â±180Â° boundary crossings
	const auto NormalizeYawDiff = [](float yawNew, float yawOld) -> float
	{
		float diff = yawNew - yawOld;
		while (diff > 180.0f) diff -= 360.0f;
		while (diff < -180.0f) diff += 360.0f;
		return diff;
	};

	if (CFG::Aimbot_Projectile_Ground_Strafe_Prediction && (m_PlayerDataBackup.m_fFlags & FL_ONGROUND) && F::LagRecords->HasRecords(pPlayer))
	{
		if (m_MoveData.m_vecVelocity.Length2D() < (m_MoveData.m_flMaxSpeed * 0.85f))
		{
			return;
		}

		const auto pRecord0 = F::LagRecords->GetRecord(pPlayer, 0);
		const auto pRecord1 = F::LagRecords->GetRecord(pPlayer, 1);
		const auto pRecord2 = F::LagRecords->GetRecord(pPlayer, 2);
		const auto pRecord3 = F::LagRecords->GetRecord(pPlayer, 3);
		const auto pRecord4 = F::LagRecords->GetRecord(pPlayer, 4);

		if (pRecord0 && pRecord1 && pRecord2 && pRecord3 && pRecord4)
		{
			const float flYaw0 = Math::VelocityToAngles(pRecord0->Velocity).y;
			const float flYaw1 = Math::VelocityToAngles(pRecord1->Velocity).y;
			const float flYaw2 = Math::VelocityToAngles(pRecord2->Velocity).y;
			const float flYaw3 = Math::VelocityToAngles(pRecord3->Velocity).y;
			const float flYaw4 = Math::VelocityToAngles(pRecord4->Velocity).y;

			const float d10 = NormalizeYawDiff(flYaw1, flYaw0);
			const float d21 = NormalizeYawDiff(flYaw2, flYaw1);
			const float d32 = NormalizeYawDiff(flYaw3, flYaw2);
			const float d43 = NormalizeYawDiff(flYaw4, flYaw3);

			const auto inc{d43 > 0.0f && d32 > 0.0f && d21 > 0.0f && d10 > 0.0f};
			const auto dec{d43 < 0.0f && d32 < 0.0f && d21 < 0.0f && d10 < 0.0f};

			if (!inc && !dec)
			{
				return;
			}

			const float flTotalYawChange = (-d10) + (-d21) + (-d32) + (-d43);
			const float flTimeSpan = std::max(pRecord0->SimulationTime - pRecord4->SimulationTime, TICK_INTERVAL);
			const float flYawRate = flTotalYawChange / 4.0f / flTimeSpan * TICK_INTERVAL;

			if (fabsf(flYawRate) < 1.0f)
			{
				return;
			}

			m_flYawTurnRate = std::clamp(flYawRate, -4.3f, 4.3f);
		}
	}

	if (CFG::Aimbot_Projectile_Air_Strafe_Prediction && !(m_PlayerDataBackup.m_fFlags & FL_ONGROUND) && F::LagRecords->HasRecords(pPlayer))
	{
		const LagRecord_t* rec0{F::LagRecords->GetRecord(pPlayer, 0)};
		const LagRecord_t* rec1{F::LagRecords->GetRecord(pPlayer, 1)};
		const LagRecord_t* rec2{F::LagRecords->GetRecord(pPlayer, 2)};
		const LagRecord_t* rec3{F::LagRecords->GetRecord(pPlayer, 3)};
		const LagRecord_t* rec4{F::LagRecords->GetRecord(pPlayer, 4)};

		if (rec0 && rec1 && rec2 && rec3 && rec4)
		{
			const float yaw0 = Math::VelocityToAngles(rec0->Velocity).y;
			const float yaw1 = Math::VelocityToAngles(rec1->Velocity).y;
			const float yaw2 = Math::VelocityToAngles(rec2->Velocity).y;
			const float yaw3 = Math::VelocityToAngles(rec3->Velocity).y;
			const float yaw4 = Math::VelocityToAngles(rec4->Velocity).y;

			const float ad10 = NormalizeYawDiff(yaw1, yaw0);
			const float ad21 = NormalizeYawDiff(yaw2, yaw1);
			const float ad32 = NormalizeYawDiff(yaw3, yaw2);
			const float ad43 = NormalizeYawDiff(yaw4, yaw3);

			const bool inc{ad43 > 0.0f && ad32 > 0.0f && ad21 > 0.0f && ad10 > 0.0f};
			const bool dec{ad43 < 0.0f && ad32 < 0.0f && ad21 < 0.0f && ad10 < 0.0f};

			if (!inc && !dec)
			{
				return;
			}

			const float delta = ((-ad10) + (-ad21) + (-ad32) + (-ad43)) / 4.0f;

			m_flYawTurnRate = delta;

			if (m_flYawTurnRate > 0.0f)
			{
				m_MoveData.m_flSideMove = -450.0f;
			}

			if (m_flYawTurnRate < 0.0f)
			{
				m_MoveData.m_flSideMove = 450.0f;
			}

			m_MoveData.m_flForwardMove = 0.0f;
		}
	}
}

bool CMovementSimulation::Initialize(C_TFPlayer* pPlayer)
{
	if (!pPlayer || pPlayer->deadflag())
		return false;

	//set player
	m_pPlayer = pPlayer;

	//set current command
	//we'll use this to set current player's command, without it CGameMovement::CheckInterval will try to access a nullptr
	static CUserCmd dummyCmd = {};

	I::MoveHelper->SetHost(m_pPlayer);
	m_pPlayer->SetCurrentCommand(&dummyCmd);

	//store player's data
	m_PlayerDataBackup.Store(m_pPlayer);

	//store vars
	m_bOldInPrediction = I::Prediction->m_bInPrediction;
	m_bOldFirstTimePredicted = I::Prediction->m_bFirstTimePredicted;
	m_flOldFrametime = I::GlobalVars->frametime;

	//the hacks that make it work
	{
		if (pPlayer->m_fFlags() & FL_DUCKING)
		{
			pPlayer->m_fFlags() &= ~FL_DUCKING; //breaks origin's z if FL_DUCKING is not removed
			pPlayer->m_bDucked() = true; //(mins/maxs will be fine when ducking as long as m_bDucked is true)
			pPlayer->m_flDucktime() = 0.0f;
			pPlayer->m_flDuckJumpTime() = 0.0f;
			pPlayer->m_bDucking() = false;
			pPlayer->m_bInDuckJump() = true;
		}

		if (pPlayer != H::Entities->GetLocal())
			pPlayer->m_hGroundEntity() = nullptr; //without this nonlocal entities get snapped to the floor

		pPlayer->m_flModelScale() -= 0.03125f; //fixes issues with corners

		if (pPlayer->m_fFlags() & FL_ONGROUND)
			pPlayer->m_vecOrigin().z += 0.03125f * 3.0f; //to prevent getting stuck in the ground

		//for some reason if xy vel is zero it doesn't predict
		if (fabsf(pPlayer->m_vecVelocity().x) < 0.01f)
			pPlayer->m_vecVelocity().x = 0.015f;

		if (fabsf(pPlayer->m_vecVelocity().y) < 0.01f)
			pPlayer->m_vecVelocity().y = 0.015f;

		if ((pPlayer->m_fFlags() & FL_ONGROUND) || pPlayer->m_hGroundEntity().Get())
		{
			pPlayer->m_vecVelocity().z = 0.0f;
		}
	}

	//setup move data
	SetupMoveData(m_pPlayer, &m_MoveData);

	return true;
}

void CMovementSimulation::Restore()
{
	if (!m_pPlayer)
		return;

	I::MoveHelper->SetHost(nullptr);
	m_pPlayer->SetCurrentCommand(nullptr);

	m_PlayerDataBackup.Restore(m_pPlayer);

	I::Prediction->m_bInPrediction = m_bOldInPrediction;
	I::Prediction->m_bFirstTimePredicted = m_bOldFirstTimePredicted;
	I::GlobalVars->frametime = m_flOldFrametime;

	m_pPlayer = nullptr;
	m_flYawTurnRate = 0.0f;
	m_vAccelTrend = {};
	m_vAdaptiveVelocity = {};
	m_vMethod2Accel = {};
	m_vMethod2Velocity = {};
	m_vMethod3OriginalVelocity = {};

	std::memset(&m_MoveData, 0, sizeof(CMoveData));
	std::memset(&m_PlayerDataBackup, 0, sizeof(CPlayerDataBackup));
}

void CMovementSimulation::RunTick(float flTimeToTarget)
{
	if (!m_pPlayer)
	{
		return;
	}

	//make sure frametime and prediction vars are right
	I::Prediction->m_bInPrediction = true;
	I::Prediction->m_bFirstTimePredicted = false;
	I::GlobalVars->frametime = I::Prediction->m_bEnginePaused ? 0.0f : TICK_INTERVAL;

	if (m_MoveData.m_vecVelocity.Length() < 15.0f && (m_pPlayer->m_fFlags() & FL_ONGROUND))
	{
		return;
	}

	if (CFG::Aimbot_Projectile_Ground_Strafe_Prediction && (m_PlayerDataBackup.m_fFlags & FL_ONGROUND) && (m_pPlayer->m_fFlags() & FL_ONGROUND))
	{
		m_MoveData.m_vecViewAngles.y += m_flYawTurnRate * Math::RemapValClamped(flTimeToTarget, 0.0f, 1.0f, 1.0f, 0.5f);
	}

	if (CFG::Aimbot_Projectile_Air_Strafe_Prediction && !(m_PlayerDataBackup.m_fFlags & FL_ONGROUND) && !(m_pPlayer->m_fFlags() & FL_ONGROUND))
	{
		m_MoveData.m_vecViewAngles.y += m_flYawTurnRate;
	}

	// Method 2: per-tick velocity extrapolation â€” advance predicted velocity by
	// the acceleration trend and recompute movement inputs each tick
	if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 2 && m_vMethod2Velocity.Length2D() > 1.0f)
	{
		m_vMethod2Velocity.x += m_vMethod2Accel.x;
		m_vMethod2Velocity.y += m_vMethod2Accel.y;

		// Clamp to max speed so prediction doesn't diverge
		const float flPredSpeed = m_vMethod2Velocity.Length2D();
		if (flPredSpeed > m_MoveData.m_flMaxSpeed * 1.1f && flPredSpeed > 0.001f)
		{
			const float flScale = m_MoveData.m_flMaxSpeed * 1.1f / flPredSpeed;
			m_vMethod2Velocity.x *= flScale;
			m_vMethod2Velocity.y *= flScale;
		}

		const Vec3 vNewAngles = { 0.0f, Math::VelocityToAngles(m_vMethod2Velocity).y, 0.0f };
		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(vNewAngles, &vForward, &vRight, nullptr);

		if (fabsf(vRight.x) > 0.001f)
		{
			const float flRatio = vRight.y / vRight.x;
			const float flDenom = vForward.y - flRatio * vForward.x;

			if (fabsf(flDenom) > 0.001f)
				m_MoveData.m_flForwardMove = (m_vMethod2Velocity.y - flRatio * m_vMethod2Velocity.x) / flDenom;
			else
				m_MoveData.m_flForwardMove = 0.0f;

			m_MoveData.m_flSideMove = (m_vMethod2Velocity.x - vForward.x * m_MoveData.m_flForwardMove) / vRight.x;
		}

		m_MoveData.m_vecViewAngles = vNewAngles;
		m_MoveData.m_vecAngles = vNewAngles;
	}

	// Method 3: per-tick adaptive velocity update - advance predicted velocity by
	// the acceleration trend and recompute movement inputs to match
	if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 3 && m_vAdaptiveVelocity.Length2D() > 1.0f)
	{
		m_vAdaptiveVelocity.x += m_vAccelTrend.x;
		m_vAdaptiveVelocity.y += m_vAccelTrend.y;

	// Clamp to max speed so prediction doesn't diverge
		const float flPredSpeed = m_vAdaptiveVelocity.Length2D();
		if (flPredSpeed > m_MoveData.m_flMaxSpeed * 1.1f && flPredSpeed > 0.001f)
		{
			const float flScale = m_MoveData.m_flMaxSpeed * 1.1f / flPredSpeed;
			m_vAdaptiveVelocity.x *= flScale;
			m_vAdaptiveVelocity.y *= flScale;
		}

		// Drift limiter: if the velocity direction has accumulated >15Â° from the original,
		// dampen the accel trend to prevent runaway directional drift from noise
		if (m_vMethod3OriginalVelocity.Length2D() > 1.0f && flPredSpeed > 0.001f)
		{
			const float flDot = (m_vAdaptiveVelocity.x * m_vMethod3OriginalVelocity.x
								+ m_vAdaptiveVelocity.y * m_vMethod3OriginalVelocity.y)
								/ (flPredSpeed * m_vMethod3OriginalVelocity.Length2D());
			const float flAngleDeg = RAD2DEG(acosf(std::clamp(flDot, -1.0f, 1.0f)));

			if (flAngleDeg > 15.0f)
			{
				// Freeze further acceleration; velocity direction has drifted too far
				m_vAccelTrend.x = 0.0f;
				m_vAccelTrend.y = 0.0f;
			}
		}

		const Vec3 vNewAngles = { 0.0f, Math::VelocityToAngles(m_vAdaptiveVelocity).y, 0.0f };
		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(vNewAngles, &vForward, &vRight, nullptr);

		if (fabsf(vRight.x) > 0.001f)
		{
			const float flRatio = vRight.y / vRight.x;
			const float flDenom = vForward.y - flRatio * vForward.x;

			if (fabsf(flDenom) > 0.001f)
				m_MoveData.m_flForwardMove = (m_vAdaptiveVelocity.y - flRatio * m_vAdaptiveVelocity.x) / flDenom;
			else
				m_MoveData.m_flForwardMove = 0.0f;

			m_MoveData.m_flSideMove = (m_vAdaptiveVelocity.x - vForward.x * m_MoveData.m_flForwardMove) / vRight.x;
		}

		m_MoveData.m_vecViewAngles = vNewAngles;
		m_MoveData.m_vecAngles = vNewAngles;
	}

	m_bRunning = true;

	I::GameMovement->ProcessMovement(m_pPlayer, &m_MoveData);

	m_bRunning = false;
}
