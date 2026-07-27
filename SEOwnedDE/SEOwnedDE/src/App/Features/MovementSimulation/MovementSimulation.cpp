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

	if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 1)
	{
		// Acceleration-based: regress recent lag-record velocities over time,
		// then update movement inputs each simulation tick with drift limiting.
		m_vAccelTrend = {};
		m_vAdaptiveVelocity = pMoveData->m_vecVelocity;

		if (F::LagRecords->HasRecords(pPlayer))
		{
			const LagRecord_t* records[] =
			{
				F::LagRecords->GetRecord(pPlayer, 0),
				F::LagRecords->GetRecord(pPlayer, 1),
				F::LagRecords->GetRecord(pPlayer, 2),
				F::LagRecords->GetRecord(pPlayer, 3),
				F::LagRecords->GetRecord(pPlayer, 4)
			};

			const LagRecord_t* rec0 = records[0];
			const LagRecord_t* rec1 = records[1];
			if (rec0 && rec1)
			{
				m_vAdaptiveVelocity = rec0->Velocity;

				int nRecords = 0;
				float flSumTime = 0.0f;
				float flSumTimeSq = 0.0f;
				float flSumVelX = 0.0f;
				float flSumVelY = 0.0f;
				float flSumTimeVelX = 0.0f;
				float flSumTimeVelY = 0.0f;

				for (const LagRecord_t* record : records)
				{
					if (!record)
						continue;

					const float flAge = rec0->SimulationTime - record->SimulationTime;
					if (flAge < -TICK_INTERVAL || flAge > TICK_INTERVAL * 8.0f)
						continue;

					const float flTime = -std::max(flAge, 0.0f);
					flSumTime += flTime;
					flSumTimeSq += flTime * flTime;
					flSumVelX += record->Velocity.x;
					flSumVelY += record->Velocity.y;
					flSumTimeVelX += flTime * record->Velocity.x;
					flSumTimeVelY += flTime * record->Velocity.y;
					nRecords++;
				}

				if (nRecords >= 3)
				{
					const float flDenom = static_cast<float>(nRecords) * flSumTimeSq - flSumTime * flSumTime;
					if (fabsf(flDenom) > 0.0001f)
					{
						const float flAccelX = (static_cast<float>(nRecords) * flSumTimeVelX - flSumTime * flSumVelX) / flDenom;
						const float flAccelY = (static_cast<float>(nRecords) * flSumTimeVelY - flSumTime * flSumVelY) / flDenom;
						const float flRecordConfidence = std::clamp(static_cast<float>(nRecords - 2) / 3.0f, 0.35f, 1.0f);

						m_vAccelTrend.x = flAccelX * TICK_INTERVAL * flRecordConfidence;
						m_vAccelTrend.y = flAccelY * TICK_INTERVAL * flRecordConfidence;
					}
				}
				else
				{
					const float dt = std::max(rec0->SimulationTime - rec1->SimulationTime, TICK_INTERVAL);
					m_vAccelTrend.x = (rec0->Velocity.x - rec1->Velocity.x) / dt * TICK_INTERVAL * 0.35f;
					m_vAccelTrend.y = (rec0->Velocity.y - rec1->Velocity.y) / dt * TICK_INTERVAL * 0.35f;
				}

				m_vAccelTrend.z = 0.0f;
			}

			const float flAccelMag = m_vAccelTrend.Length2D();
			const float flMaxAccel = pMoveData->m_flMaxSpeed * 0.15f;
			if (flAccelMag > flMaxAccel && flAccelMag > 0.001f)
			{
				m_vAccelTrend.x *= flMaxAccel / flAccelMag;
				m_vAccelTrend.y *= flMaxAccel / flAccelMag;
			}
		}

		if (m_vAdaptiveVelocity.Length2D() > 1.0f)
			pMoveData->m_vecViewAngles = { 0.0f, Math::VelocityToAngles(m_vAdaptiveVelocity).y, 0.0f };

		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(pMoveData->m_vecViewAngles, &vForward, &vRight, nullptr);

		MovementPredictionMath::ProjectHorizontalVelocity(m_vAdaptiveVelocity, vForward, vRight,
			pMoveData->m_flForwardMove, pMoveData->m_flSideMove);
	}

	else
	{
		// Linear: decompose current velocity into forward/side move
		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(pMoveData->m_vecViewAngles, &vForward, &vRight, nullptr);

		MovementPredictionMath::ProjectHorizontalVelocity(pMoveData->m_vecVelocity, vForward, vRight,
			pMoveData->m_flForwardMove, pMoveData->m_flSideMove);
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

	UpdateYawTurnRate(pPlayer);
}

void CMovementSimulation::ResetMoveHistory(int iEntIndex, int iEntityHandle, float flDeathTime)
{
	if (iEntIndex < 1 || iEntIndex >= MAX_PLAYERS)
		return;

	m_aMoveRecordHead[iEntIndex] = 0;
	m_aMoveRecordCount[iEntIndex] = 0;
	m_aMoveHistoryStates[iEntIndex] = {};
	m_aMoveHistoryStates[iEntIndex].m_iEntityHandle = iEntityHandle;
	m_aMoveHistoryStates[iEntIndex].m_flDeathTime = flDeathTime;
	m_aMoveHistoryStates[iEntIndex].m_bInitialized = true;
}

bool CMovementSimulation::PrepareMoveHistory(C_TFPlayer* pPlayer, int& iEntIndex)
{
	if (!pPlayer)
		return false;

	iEntIndex = pPlayer->entindex();
	if (iEntIndex < 1 || iEntIndex >= MAX_PLAYERS)
		return false;

	const int iEntityHandle = pPlayer->GetRefEHandle().ToInt();
	const float flDeathTime = pPlayer->m_flDeathTime();
	const float flSimulationTime = pPlayer->m_flSimulationTime();
	auto& state = m_aMoveHistoryStates[iEntIndex];

	bool bReset = !state.m_bInitialized
		|| state.m_iEntityHandle != iEntityHandle
		|| state.m_flDeathTime != flDeathTime;

	const float flLastHistoryTime = std::max(state.m_flLastRecordTime, state.m_flLastYawTime);
	if (!bReset && flLastHistoryTime > 0.0f)
	{
		const float flElapsedTime = flSimulationTime - flLastHistoryTime;
		bReset = flElapsedTime < -TICK_INTERVAL * 0.5f;
	}

	if (bReset)
		ResetMoveHistory(iEntIndex, iEntityHandle, flDeathTime);

	return true;
}

void CMovementSimulation::UpdateYawTurnRate(C_TFPlayer* pPlayer)
{
	m_flYawTurnRate = 0.0f;

	int iEntIndex = 0;
	if (!PrepareMoveHistory(pPlayer, iEntIndex))
		return;

	auto& state = m_aMoveHistoryStates[iEntIndex];
	if (pPlayer->m_vecVelocity().Length2D() <= 10.0f)
	{
		state.m_bHasLastYaw = false;
		state.m_flYawTurnRate = 0.0f;
		return;
	}

	const float flCurrentYaw = Math::VelocityToAngles(pPlayer->m_vecVelocity()).y;
	const float flSimulationTime = pPlayer->m_flSimulationTime();
	const float flElapsedTime = flSimulationTime - state.m_flLastYawTime;
	const float flMaxHistoryGap = TICK_INTERVAL * static_cast<float>(MAX_HISTORY_GAP_TICKS);
	if (state.m_bHasLastYaw && flElapsedTime > flMaxHistoryGap)
	{
		state.m_bHasLastYaw = false;
		state.m_flYawTurnRate = 0.0f;
	}
	else if (state.m_bHasLastYaw && flElapsedTime > 0.0f)
	{
		state.m_flYawTurnRate = MovementPredictionMath::ComputeYawDeltaPerTick(
			flCurrentYaw, state.m_flLastYaw, flElapsedTime, TICK_INTERVAL);
	}

	state.m_flLastYaw = flCurrentYaw;
	state.m_flLastYawTime = flSimulationTime;
	state.m_bHasLastYaw = true;
	m_flYawTurnRate = state.m_flYawTurnRate;
}

void CMovementSimulation::StoreMoveRecord(C_TFPlayer* pPlayer)
{
	if (!pPlayer)
		return;

	int iEntIndex = 0;
	if (!PrepareMoveHistory(pPlayer, iEntIndex))
		return;

	if (pPlayer->deadflag())
	{
		ResetMoveHistory(iEntIndex, pPlayer->GetRefEHandle().ToInt(), pPlayer->m_flDeathTime());
		return;
	}

	auto& state = m_aMoveHistoryStates[iEntIndex];
	const float flSimulationTime = pPlayer->m_flSimulationTime();
	if (state.m_flLastRecordTime > 0.0f)
	{
		const float flElapsedTime = flSimulationTime - state.m_flLastRecordTime;
		const float flMaxHistoryGap = TICK_INTERVAL * static_cast<float>(MAX_HISTORY_GAP_TICKS);
		if (flElapsedTime < -TICK_INTERVAL * 0.5f || flElapsedTime > flMaxHistoryGap)
			ResetMoveHistory(iEntIndex, pPlayer->GetRefEHandle().ToInt(), pPlayer->m_flDeathTime());
		else if (flElapsedTime <= 0.0f)
			return;
	}

	// Push a new "logical 0" (most recent) into the ring: step head back one slot
	// (wrapping), write there, and grow the count up to capacity.
	uint8_t& iHead = m_aMoveRecordHead[iEntIndex];
	iHead = static_cast<uint8_t>((iHead + MOVE_RECORD_CAPACITY - 1) % MOVE_RECORD_CAPACITY);

	MoveRecord_t& record = m_aMoveRecords[iEntIndex][iHead];
	record.m_vVelocity = pPlayer->m_vecVelocity();
	record.m_bHasDirection = pPlayer->m_vecVelocity().Length2D() > 1.0f;
	record.m_vDirection = record.m_bHasDirection ? Math::VelocityToAngles(pPlayer->m_vecVelocity()) : Vec3{};
	record.m_flSimTime = flSimulationTime;
	record.m_iFlags = pPlayer->m_fFlags();
	state.m_flLastRecordTime = flSimulationTime;

	if (m_aMoveRecordCount[iEntIndex] < MOVE_RECORD_CAPACITY)
		m_aMoveRecordCount[iEntIndex]++;
}

float CMovementSimulation::CalculateHitchance(C_TFPlayer* pPlayer, int iSamples)
{
	if (!pPlayer)
		return 0.0f;

	int iEntIndex = 0;
	if (!PrepareMoveHistory(pPlayer, iEntIndex))
		return 0.0f;

	const float flLastRecordTime = m_aMoveHistoryStates[iEntIndex].m_flLastRecordTime;
	if (flLastRecordTime > 0.0f
		&& pPlayer->m_flSimulationTime() - flLastRecordTime > TICK_INTERVAL * static_cast<float>(MAX_HISTORY_GAP_TICKS))
	{
		ResetMoveHistory(iEntIndex, pPlayer->GetRefEHandle().ToInt(), pPlayer->m_flDeathTime());
	}

	iSamples = std::max(iSamples, 1);
	const int iStored = GetMoveRecordCount(iEntIndex);
	if (iStored < 3)
		return 1.0f;

	const auto iRecordCount = std::min(iStored, 30);
	if (iRecordCount < 3)
		return 1.0f;

	float flCurrentChance = 1.0f;
	float flAverageYaw = 0.0f;
	int iSampleCount = 0;

	for (int i = 0; i < iRecordCount - 1; i++)
	{
		if (i + 1 >= iStored)
			break;

		const auto& record1 = GetMoveRecord(iEntIndex, i);
		const auto& record2 = GetMoveRecord(iEntIndex, i + 1);

		if (!record1.m_bHasDirection || !record2.m_bHasDirection)
			continue;

		const float flYaw1 = record1.m_vDirection.y;
		const float flYaw2 = record2.m_vDirection.y;
		const float flTimeDelta = std::max(record1.m_flSimTime - record2.m_flSimTime, TICK_INTERVAL);
		const int iTicks = std::max(TIME_TO_TICKS(flTimeDelta), 1);

		float flYawChange = Math::NormalizeAngle(flYaw1 - flYaw2) / static_cast<float>(iTicks);

		flAverageYaw += flYawChange;
		iSampleCount++;

		if ((i + 1) % iSamples == 0 || i == iRecordCount - 2)
		{
			const float flSampleAvg = flAverageYaw / static_cast<float>(iSampleCount);

			auto& state = m_aMoveHistoryStates[iEntIndex];
			const float flExpected = state.m_flExpectedYaw;

			if (state.m_bHasExpectedYaw && fabsf(flExpected - flSampleAvg) > 1.5f)
			{
				const float flPenalty = 1.0f / (static_cast<float>((iRecordCount - 1) / iSamples) + 1.0f);
				flCurrentChance -= flPenalty;
			}

			state.m_flExpectedYaw = flSampleAvg;
			state.m_bHasExpectedYaw = true;
			flAverageYaw = 0.0f;
			iSampleCount = 0;
		}
	}

	return std::clamp(flCurrentChance, 0.0f, 1.0f);
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

	const float flStrafeYawStep = GetStrafeYawStep(flTimeToTarget);
	const bool bStrafePredictionActive = IsStrafePredictionActive();
	if (CFG::Aimbot_Projectile_Aim_Prediction_Method != 1 && bStrafePredictionActive)
		m_MoveData.m_vecViewAngles.y = Math::NormalizeAngle(m_MoveData.m_vecViewAngles.y + flStrafeYawStep);

	// Acceleration-based prediction: advance predicted velocity by the
	// regression-derived accel trend and recompute movement inputs to match
	if (CFG::Aimbot_Projectile_Aim_Prediction_Method == 1 && m_vAdaptiveVelocity.Length2D() > 1.0f)
	{
		const Vec3 vPreviousAdaptiveVelocity = m_vAdaptiveVelocity;
		m_vAdaptiveVelocity.x += m_vAccelTrend.x;
		m_vAdaptiveVelocity.y += m_vAccelTrend.y;

		float flPredSpeed = m_vAdaptiveVelocity.Length2D();
		if (flPredSpeed > m_MoveData.m_flMaxSpeed * 1.1f && flPredSpeed > 0.001f)
		{
			const float flScale = m_MoveData.m_flMaxSpeed * 1.1f / flPredSpeed;
			m_vAdaptiveVelocity.x *= flScale;
			m_vAdaptiveVelocity.y *= flScale;
			flPredSpeed = m_vAdaptiveVelocity.Length2D();
		}

		const float flPreviousSpeed = vPreviousAdaptiveVelocity.Length2D();
		if (flPreviousSpeed > 1.0f && flPredSpeed > 0.001f)
		{
			const float flDot = (m_vAdaptiveVelocity.x * vPreviousAdaptiveVelocity.x
								+ m_vAdaptiveVelocity.y * vPreviousAdaptiveVelocity.y)
								/ (flPredSpeed * flPreviousSpeed);
			const float flAngleDeg = RAD2DEG(acosf(std::clamp(flDot, -1.0f, 1.0f)));

			if (flAngleDeg > 15.0f)
			{
				m_vAccelTrend.x = 0.0f;
				m_vAccelTrend.y = 0.0f;
				m_vAdaptiveVelocity = vPreviousAdaptiveVelocity;
			}
		}

		if (bStrafePredictionActive)
		{
			m_vAdaptiveVelocity = MovementPredictionMath::ReconcileHorizontalTurn(
				vPreviousAdaptiveVelocity, m_vAdaptiveVelocity, flStrafeYawStep);
		}

		const Vec3 vNewAngles = { 0.0f, Math::VelocityToAngles(m_vAdaptiveVelocity).y, 0.0f };
		Vec3 vForward = {}, vRight = {};
		Math::AngleVectors(vNewAngles, &vForward, &vRight, nullptr);

		MovementPredictionMath::ProjectHorizontalVelocity(m_vAdaptiveVelocity, vForward, vRight,
			m_MoveData.m_flForwardMove, m_MoveData.m_flSideMove);

		m_MoveData.m_vecViewAngles = vNewAngles;
		m_MoveData.m_vecAngles = vNewAngles;
	}


	m_bRunning = true;

	I::GameMovement->ProcessMovement(m_pPlayer, &m_MoveData);

	m_bRunning = false;
}

bool CMovementSimulation::IsStrafePredictionActive() const
{
	if (CFG::Aimbot_Projectile_Ground_Strafe_Prediction
		&& (m_PlayerDataBackup.m_fFlags & FL_ONGROUND) && (m_pPlayer->m_fFlags() & FL_ONGROUND))
		return true;

	if (CFG::Aimbot_Projectile_Air_Strafe_Prediction
		&& !(m_PlayerDataBackup.m_fFlags & FL_ONGROUND) && !(m_pPlayer->m_fFlags() & FL_ONGROUND))
		return true;

	return false;
}

float CMovementSimulation::GetStrafeYawStep(float flTimeToTarget) const
{
	if (!IsStrafePredictionActive())
		return 0.0f;

	if ((m_PlayerDataBackup.m_fFlags & FL_ONGROUND) && (m_pPlayer->m_fFlags() & FL_ONGROUND))
		return m_flYawTurnRate * Math::RemapValClamped(flTimeToTarget, 0.0f, 1.0f, 1.0f, 0.5f);

	return m_flYawTurnRate;
}
