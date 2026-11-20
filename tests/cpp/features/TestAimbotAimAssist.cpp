#include <gtest/gtest.h>

#include "SDK/Helpers/AimUtils/AimUtils.h"
#include "App/Features/Aimbot/AimbotHitscan/AimAssistController.h"

#include <limits>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHitscanSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp";
constexpr const char* kHitscanHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.h";
constexpr const char* kAimbotSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/Aimbot.cpp";
constexpr const char* kCreateMoveSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/ClientModeShared_CreateMove.cpp";
constexpr const char* kCfgHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/CFG.h";
constexpr const char* kAimUtilsSource = "SEOwnedDE/SEOwnedDE/src/SDK/Helpers/AimUtils/AimUtils.cpp";
}

TEST(AimbotAimAssistContracts, IndependentAssistRunsBeforeHitscanKeyGate) {
    const auto root = testhelpers::FindRepoRoot();
    const auto sourcePath = root / kHitscanSource;

    ASSERT_TRUE(std::filesystem::exists(sourcePath));

    const auto source = testhelpers::ReadTextFile(sourcePath);
    EXPECT_EQ(source.find("case 3:"), std::string::npos);
    const auto runStart = source.find("void CAimbotHitscan::Run(CUserCmd*");
    const auto assist = source.find("RunAimAssist(pCmd, pLocal, pWeapon);", runStart);
    const auto regularGate = source.find("if (!CFG::Aimbot_Hitscan_Active)", runStart);
    ASSERT_NE(assist, std::string::npos);
    ASSERT_NE(regularGate, std::string::npos);
    EXPECT_LT(assist, regularGate);
    EXPECT_NE(source.find("Aimbot_Hitscan_AimAssist_Active && !bRegularAimActive"), std::string::npos);
    EXPECT_NE(source.find("Aimbot_Hitscan_AimAssist_Strength"), std::string::npos);
    EXPECT_NE(source.find("Aimbot_Hitscan_AimAssist_Stabilization"), std::string::npos);
    const auto assistStart = source.find("void CAimbotHitscan::RunAimAssist(");
    const auto assistEnd = source.find("bool CAimbotHitscan::IsFiring(", assistStart);
    ASSERT_NE(assistStart, std::string::npos);
    ASSERT_NE(assistEnd, std::string::npos);
    ASSERT_GT(assistEnd, assistStart);
    const auto assistBody = source.substr(assistStart, assistEnd - assistStart);
    EXPECT_NE(assistBody.find("m_AssistLock.BeginCommand"), std::string::npos);
    EXPECT_NE(assistBody.find("GetRefEHandle().ToInt()"), std::string::npos);
    EXPECT_NE(assistBody.find("Math::RayToHitbox"), std::string::npos);
    const auto crosshairContact = assistBody.find("ConsiderHitbox(crosshairTrace.hitbox);");
    const auto retainedBone = assistBody.find("ConsiderHitbox(m_nAssistHitbox)", crosshairContact);
    ASSERT_NE(crosshairContact, std::string::npos);
    ASSERT_NE(retainedBone, std::string::npos);
    EXPECT_LT(crosshairContact, retainedBone);
    EXPECT_NE(assistBody.find("position = crosshairTrace.endpos;"), std::string::npos);
    EXPECT_NE(assistBody.find("if (!bSafeCenter && !onCrosshair)"), std::string::npos);
    EXPECT_NE(assistBody.find("I::EngineClient->SetViewAngles"), std::string::npos);
    EXPECT_EQ(assistBody.find("tick_count"), std::string::npos);
    EXPECT_EQ(assistBody.find("IN_ATTACK"), std::string::npos);
    EXPECT_EQ(assistBody.find("CLagRecordScope"), std::string::npos);
}

TEST(AimbotAimAssistContracts, CfgDeclaresAimAssistTuningVar) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cfgPath = root / kCfgHeader;

    ASSERT_TRUE(std::filesystem::exists(cfgPath));

    const auto cfgText = testhelpers::ReadTextFile(cfgPath);
    EXPECT_NE(cfgText.find("CFGVAR(Aimbot_Hitscan_AimAssist_Active, true)"), std::string::npos);
    EXPECT_NE(cfgText.find("CFGVAR(Aimbot_Hitscan_AimAssist_Strength"), std::string::npos);
    EXPECT_NE(cfgText.find("CFGVAR(Aimbot_Hitscan_AimAssist_Stabilization"), std::string::npos);
}

TEST(AimbotAimAssistSelection, CrosshairContactWinsAcquisitionAndVisibleLockWinsRetention) {
    HitscanAssist::Candidate candidates[] = {
        { 101, 0.1f, false }, { 202, 2.5f, true }, { 303, 24.0f, false }
    };
    EXPECT_EQ(HitscanAssist::SelectCandidate(candidates, 3, -1, 3.0f), 1);
    EXPECT_EQ(HitscanAssist::SelectCandidate(candidates, 3, 303, 3.0f), 2);

    // Death, invisibility, off-screen points and occlusion make the native
    // candidate invalid before selection; a contact then takes priority again.
    candidates[2].Valid = false;
    EXPECT_EQ(HitscanAssist::SelectCandidate(candidates, 3, 303, 3.0f), 1);
}

TEST(AimbotAimAssistSelection, InvalidAndDistantCandidatesCannotAcquire) {
    const HitscanAssist::Candidate candidates[] = {
        { 101, 3.1f, false }, { 202, std::numeric_limits<float>::quiet_NaN(), true },
        { -1, 0.0f, true }, { 404, -1.0f, false }, { 505, 0.5f, true, false }
    };
    EXPECT_EQ(HitscanAssist::SelectCandidate(candidates, 5, -1, 3.0f), -1);
}

TEST(AimbotAimAssistSelection, CandidateMustPersistAndSwitchWaitsAgain) {
    HitscanAssist::TargetLock lock;
    HitscanAssist::Candidate candidates[] = { { 101, 1.0f, false } };
    for (int command = 1; command <= 6; ++command) {
        lock.BeginCommand(command, 901);
        EXPECT_EQ(lock.Select(candidates, 1, 3.0f, 0.015f), -1);
    }
    lock.BeginCommand(7, 901);
    EXPECT_EQ(lock.Select(candidates, 1, 3.0f, 0.015f), 0);
    EXPECT_EQ(lock.GetHandle(), 101);

    // A retained target can move outside the acquisition cone while onscreen.
    candidates[0].FOV = 20.0f;
    lock.BeginCommand(8, 901);
    EXPECT_EQ(lock.Select(candidates, 1, 3.0f, 0.015f), 0);

    candidates[0] = { 202, 0.25f, true };
    for (int command = 9; command <= 14; ++command) {
        lock.BeginCommand(command, 901);
        EXPECT_EQ(lock.Select(candidates, 1, 3.0f, 0.015f), -1);
    }
    lock.BeginCommand(15, 901);
    EXPECT_EQ(lock.Select(candidates, 1, 3.0f, 0.015f), 0);
    EXPECT_EQ(lock.GetHandle(), 202);
}

TEST(AimbotAimAssistSelection, SkippedRepeatedCommandsAndWeaponChangesDiscardLock) {
    HitscanAssist::TargetLock lock;
    const HitscanAssist::Candidate candidates[] = { { 101, 0.5f, true } };
    for (int command = 1; command <= 7; ++command) {
        lock.BeginCommand(command, 901);
        lock.Select(candidates, 1, 3.0f, 0.015f);
    }
    ASSERT_EQ(lock.GetHandle(), 101);
    EXPECT_FALSE(lock.BeginCommand(7, 901));
    EXPECT_EQ(lock.GetHandle(), -1);
    EXPECT_EQ(lock.Select(candidates, 1, 3.0f, 0.015f), -1);
    EXPECT_FALSE(lock.BeginCommand(9, 901));
    EXPECT_FALSE(lock.BeginCommand(10, 902));
    EXPECT_EQ(lock.GetHandle(), -1);
}

TEST(AimbotAimAssistSelection, RecycledEntityIndexDoesNotInheritSerialHandleLock) {
    HitscanAssist::TargetLock lock;
    HitscanAssist::Candidate candidates[] = { { 0x00010005, 0.25f, true } };
    for (int command = 1; command <= 7; ++command) {
        lock.BeginCommand(command, 901);
        lock.Select(candidates, 1, 3.0f, 0.015f);
    }
    ASSERT_EQ(lock.GetHandle(), 0x00010005);
    candidates[0].Handle = 0x00020005;
    lock.BeginCommand(8, 901);
    EXPECT_EQ(lock.Select(candidates, 1, 3.0f, 0.015f), -1);
    EXPECT_EQ(lock.GetHandle(), -1);
}

TEST(AimbotAimAssistMotion, SpeedAndAccelerationRemainGentleWithEitherStabilizationSetting) {
    constexpr float dt = 1.0f / 66.0f;
    for (bool stabilization : { false, true }) {
        HitscanAssist::Motion motion;
        HitscanAssist::AngularStep previousVelocity = {};
        for (int tick = 0; tick < 100; ++tick) {
            const auto step = motion.Step({ 4.0f, 4.0f }, dt, 15.0f, stabilization);
            EXPECT_LE(std::hypot(step.Pitch, step.Yaw), 10.0f * dt + 1e-6f);
            const HitscanAssist::AngularStep velocity = { step.Pitch / dt, step.Yaw / dt };
            EXPECT_LE(std::hypot(velocity.Pitch - previousVelocity.Pitch,
                velocity.Yaw - previousVelocity.Yaw), 60.0f * dt + 1e-5f);
            previousVelocity = velocity;
        }
    }
}

TEST(AimbotAimAssistMotion, DiagonalCorrectionsNeverOvershootEitherAxis) {
    HitscanAssist::Motion motion;
    HitscanAssist::AngularStep error = { 0.05f, -1.0f };
    for (int tick = 0; tick < 180; ++tick) {
        const auto step = motion.Step(error, 1.0f / 66.0f, 1.0f, true);
        EXPECT_GE(step.Pitch, 0.0f);
        EXPECT_LE(step.Pitch, error.Pitch + 1e-6f);
        EXPECT_LE(step.Yaw, 0.0f);
        EXPECT_GE(step.Yaw, error.Yaw - 1e-6f);
        error.Pitch -= step.Pitch;
        error.Yaw -= step.Yaw;
    }
    EXPECT_LE(std::hypot(error.Pitch, error.Yaw), 0.026f);
}

TEST(AimbotAimAssistMotion, CorrectionsUseElapsedTimeInsteadOfCommandCount) {
    auto travelInOneSecond = [](int rate) {
        HitscanAssist::Motion motion;
        float yaw = 0.0f;
        for (int tick = 0; tick < rate; ++tick)
            yaw += motion.Step({ 0.0f, 6.0f }, 1.0f / rate, 15.0f, true).Yaw;
        return yaw;
    };
    EXPECT_NEAR(travelInOneSecond(66), travelInOneSecond(128), 0.06f);
}

TEST(AimbotAimAssistMotion, ReversalsAndYawWrappingCannotContinueWrongDirection) {
    HitscanAssist::Motion motion;
    for (int tick = 0; tick < 40; ++tick)
        motion.Step({ 0.0f, 2.0f }, 1.0f / 66.0f, 15.0f, true);
    EXPECT_LE(motion.Step({ 0.0f, -2.0f }, 1.0f / 66.0f, 15.0f, true).Yaw, 0.0f);
    motion.Reset();
    EXPECT_LT(motion.Step({ 0.0f, 358.0f }, 1.0f / 66.0f, 15.0f, true).Yaw, 0.0f);
}

TEST(AimbotAimAssistMotion, DisabledInvalidAndCenteredInputsDiscardMotion) {
    HitscanAssist::Motion motion;
    motion.Step({ 1.0f, 1.0f }, 1.0f / 66.0f, 15.0f, true);
    const auto disabled = motion.Step({ 1.0f, 1.0f }, 1.0f / 66.0f, 0.0f, true);
    EXPECT_FLOAT_EQ(disabled.Pitch, 0.0f);
    EXPECT_FLOAT_EQ(disabled.Yaw, 0.0f);
    EXPECT_FLOAT_EQ(motion.Step({ 1.0f, 1.0f }, 0.1f, 15.0f, true).Yaw, 0.0f);
    EXPECT_FLOAT_EQ(motion.Step({ 0.0f, std::numeric_limits<float>::quiet_NaN() },
        1.0f / 66.0f, 15.0f, true).Yaw, 0.0f);
    const auto centered = motion.Step({ 0.01f, 0.01f }, 1.0f / 66.0f, 15.0f, true);
    EXPECT_FLOAT_EQ(centered.Pitch, 0.0f);
    EXPECT_FLOAT_EQ(centered.Yaw, 0.0f);

    HitscanAssist::Motion fresh;
    EXPECT_FLOAT_EQ(motion.Step({ 1.0f, 1.0f }, 1.0f / 66.0f, 15.0f, true).Yaw,
        fresh.Step({ 1.0f, 1.0f }, 1.0f / 66.0f, 15.0f, true).Yaw);
}

TEST(AimbotSmoothContracts, TrackingAndAngularVelocityAreStabilized) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kHitscanSource);
    const auto header = testhelpers::ReadTextFile(root / kHitscanHeader);

    EXPECT_NE(header.find("m_nSmoothTargetIndex"), std::string::npos);
    EXPECT_NE(header.find("m_vSmoothAimStep"), std::string::npos);
    EXPECT_NE(header.find("m_nLastSmoothCommandNumber"), std::string::npos);
    EXPECT_NE(header.find("ResetSmoothState()"), std::string::npos);

    EXPECT_NE(source.find("auto FindSmoothTarget"), std::string::npos);
    EXPECT_NE(source.find("const bool bIsLocked"), std::string::npos);
    EXPECT_NE(source.find("(target.LagRecord != nullptr) != bHistorical"), std::string::npos);
    EXPECT_NE(source.find("target.Entity->entindex() == m_nSmoothTargetIndex"), std::string::npos);
    EXPECT_NE(source.find("nTargetIndex != m_nSmoothTargetIndex"), std::string::npos);
    EXPECT_NE(source.find("pCmd->command_number != m_nLastSmoothCommandNumber + 1"), std::string::npos);
    EXPECT_NE(source.find("FindSmoothTarget(true, false) || FindSmoothTarget(true, true)"), std::string::npos);
    EXPECT_NE(source.find("FindSmoothTarget(false, false) || FindSmoothTarget(false, true)"), std::string::npos);
    EXPECT_EQ(source.find("std::stable_partition"), std::string::npos);

    EXPECT_GE(testhelpers::CountOccurrences(source, "if (bStablePointOrder)"), 4u);
    EXPECT_NE(source.find("flSmoothDeadzone"), std::string::npos);
    EXPECT_NE(source.find("std::max(CFG::Aimbot_Hitscan_Smoothing, 1.0f)"), std::string::npos);
    EXPECT_NE(source.find("m_vSmoothAimStep += (vDesiredStep - m_vSmoothAimStep)"), std::string::npos);
    EXPECT_NE(source.find("m_vSmoothAimStep.Dot(vDelta) <= 0.0f"), std::string::npos);
    EXPECT_NE(source.find("flStepLengthSqr > flDeltaLengthSqr"), std::string::npos);
}

TEST(AimbotAimAssistContracts, ManualShotUsesExactHistoricalRayWithoutFovGate) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kHitscanSource);
    const auto functionStart = source.find("bool CAimbotHitscan::ResolveManualShot");
    const auto functionEnd = source.find("bool CAimbotHitscan::GetTarget", functionStart);

    ASSERT_NE(functionStart, std::string::npos);
    ASSERT_NE(functionEnd, std::string::npos);

    const auto functionBody = source.substr(functionStart, functionEnd - functionStart);
    EXPECT_EQ(functionBody.find("Aimbot_Hitscan_Target_LagRecords"), std::string::npos);
    EXPECT_EQ(functionBody.find("Aimbot_Target_Players"), std::string::npos);
    EXPECT_NE(functionBody.find("pCmd->viewangles + pLocal->m_vecPunchAngle()"), std::string::npos);
    EXPECT_NE(functionBody.find("EEntGroup::PLAYERS_ENEMIES"), std::string::npos);
    EXPECT_NE(functionBody.find("CLagRecords::IsRecordUsable"), std::string::npos);
    EXPECT_NE(functionBody.find("CLagRecordScope scope(pRecord)"), std::string::npos);
    EXPECT_NE(functionBody.find("scope.IsActive()"), std::string::npos);
    EXPECT_NE(functionBody.find("H::AimUtils->TraceEntityBullet"), std::string::npos);
    EXPECT_NE(functionBody.find("HistoricalPoseMayIntersectRay"), std::string::npos);
    EXPECT_EQ(functionBody.find("Aimbot_Hitscan_FOV"), std::string::npos);
}

TEST(AimbotAimAssistContracts, ManualShotPrefersVisiblePoseThenSearchesHistory) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kHitscanSource);
    const auto functionStart = source.find("bool CAimbotHitscan::ResolveManualShot");
    const auto functionEnd = source.find("bool CAimbotHitscan::ValidateTarget", functionStart);

    ASSERT_NE(functionStart, std::string::npos);
    ASSERT_NE(functionEnd, std::string::npos);

    const auto functionBody = source.substr(functionStart, functionEnd - functionStart);
    EXPECT_EQ(functionBody.find("pRecord->SimulationTime <= pBestRecord->SimulationTime"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(functionBody, "pCmd->tick_count ="), 2u);

    // Direct hits use the visible pose; a miss searches recent history.
    const auto historicalStamp = functionBody.find("CLagRecords::GetCommandTick(pBestRecord->SimulationTime)");
    const auto accuracyGate = functionBody.find("CFG::Misc_Accuracy_Improvements");
    const auto liveStamp = functionBody.find("F::LagRecords->GetLiveCommandTick(pCmd->tick_count)");
    ASSERT_NE(historicalStamp, std::string::npos);
    ASSERT_NE(accuracyGate, std::string::npos);
    ASSERT_NE(liveStamp, std::string::npos);
    EXPECT_LT(liveStamp, historicalStamp);
    EXPECT_LT(accuracyGate, liveStamp);

    // The manual history toggle does not disable live Accuracy correction.
    const auto backtrackGate = functionBody.find("Aimbot_Hitscan_Manual_Backtrack");
    const auto firstTrace = functionBody.find("H::AimUtils->TraceEntityBullet");
    ASSERT_NE(backtrackGate, std::string::npos);
    ASSERT_NE(firstTrace, std::string::npos);
    EXPECT_LT(firstTrace, backtrackGate);
    EXPECT_EQ(testhelpers::CountOccurrences(functionBody, "H::AimUtils->TraceEntityBullet"), 2u);

    EXPECT_NE(functionBody.find("if (pBestRecord && pBestPlayer)"), std::string::npos);
    EXPECT_EQ(functionBody.find("TIME_TO_TICKS"), std::string::npos);

    const auto runStart = source.find("void CAimbotHitscan::Run");
    ASSERT_NE(runStart, std::string::npos);
    const auto runBody = source.substr(runStart);
    // Manual ownership is captured separately from auto aim-assist firing.
    EXPECT_NE(runBody.find("const bool bManualFiring = IsFiring(pCmd, pWeapon)"), std::string::npos);
    EXPECT_NE(runBody.find("G::bManualHitscanFiring = bManualFiring"), std::string::npos);
    EXPECT_NE(runBody.find("const bool bIsFiring = IsFiring(pCmd, pWeapon)"), std::string::npos);
    // Manual resolution is no longer reachable from inside Run. It moved out to
    // CAimbot::Run, past RunMain, because every early return in this function and
    // in RunMain above it silently dropped the user's backtrack (Aimbot_Active off,
    // cursor visible, cloaked, taunting, Auto Scope, the fire-delay windows, a
    // building winning the FOV sort, the minigun spin-up hack). Run's remaining job
    // is to publish ownership via bManualHitscanFiring, asserted above, so that
    // caller knows a hand-aimed shot is live.
    EXPECT_EQ(runBody.find("ResolveManualShot(pCmd, pLocal);"), std::string::npos);
    // The main aimbot mutates angles while its key is held; independent assist
    // leaves manual-shot ownership to the same resolver after its gentle nudge.
    EXPECT_NE(runBody.find("if (aimKeyDown)"), std::string::npos);
    EXPECT_NE(runBody.find("Aim(pCmd, pLocal, target.AngleTo)"), std::string::npos);
}

TEST(AimbotAimAssistContracts, HistoricalTracesBypassLiveTargetBoundsButPreserveOcclusion) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kAimUtilsSource);
    const auto helperStart = source.find("bool TraceScopedEntity");
    const auto traceStart = source.find("void CAimUtils::Trace(", helperStart);

    ASSERT_NE(helperStart, std::string::npos);
    ASSERT_NE(traceStart, std::string::npos);

    const auto helperBody = source.substr(helperStart, traceStart - helperStart);
    EXPECT_NE(helperBody.find("I::EngineTrace->ClipRayToEntity"), std::string::npos);
    EXPECT_NE(helperBody.find("blockerFilter.m_pIgnore = pEntity"), std::string::npos);
    EXPECT_NE(helperBody.find("I::EngineTrace->TraceRay"), std::string::npos);
    EXPECT_NE(helperBody.find("blockerTrace.allsolid || blockerTrace.startsolid"), std::string::npos);
    EXPECT_NE(helperBody.find("blockerTrace.fraction + TRACE_FRACTION_EPSILON < targetTrace.fraction"), std::string::npos);

    const auto bulletStart = source.find("bool CAimUtils::TraceEntityBullet");
    const auto bulletEnd = source.find("bool CAimUtils::TraceEntityAutoDet", bulletStart);
    const auto meleeStart = source.find("bool CAimUtils::TraceEntityMelee");
    const auto meleeEnd = source.find("bool CAimUtils::TracePositionWorld", meleeStart);

    ASSERT_NE(bulletStart, std::string::npos);
    ASSERT_NE(bulletEnd, std::string::npos);
    ASSERT_NE(meleeStart, std::string::npos);
    ASSERT_NE(meleeEnd, std::string::npos);

    const auto bulletBody = source.substr(bulletStart, bulletEnd - bulletStart);
    const auto meleeBody = source.substr(meleeStart, meleeEnd - meleeStart);
    EXPECT_NE(bulletBody.find("F::LagRecordMatrixHelper->IsActiveFor(pEntity)"), std::string::npos);
    EXPECT_NE(bulletBody.find("TraceScopedEntity"), std::string::npos);
    EXPECT_NE(meleeBody.find("F::LagRecordMatrixHelper->IsActiveFor(pEntity)"), std::string::npos);
    EXPECT_NE(meleeBody.find("ray.Init(vFrom, vTo, melee_hull_mins, melee_hull_maxs)"), std::string::npos);
    EXPECT_NE(meleeBody.find("TraceScopedEntity"), std::string::npos);
}

TEST(AimbotAimAssistContracts, ManualShotOwnershipIsCapturedBeforeAimbotMutation) {
    const auto root = testhelpers::FindRepoRoot();
    const auto aimbot = testhelpers::ReadTextFile(root / kAimbotSource);
    const auto createMove = testhelpers::ReadTextFile(root / kCreateMoveSource);
    const auto runStart = aimbot.find("void CAimbot::Run(CUserCmd* pCmd)");
    const auto ownershipCapture = aimbot.find("G::bManualHitscanFiring", runStart);
    const auto runMain = aimbot.find("RunMain(pCmd);", runStart);

    ASSERT_NE(runStart, std::string::npos);
    ASSERT_NE(ownershipCapture, std::string::npos);
    ASSERT_NE(runMain, std::string::npos);
    EXPECT_LT(ownershipCapture, runMain);
    EXPECT_NE(createMove.find("G::bManualHitscanFiring = false;"), std::string::npos);
}

TEST(AimUtilsMovement, DirectRotationPreservesMovementAndUpMove) {
    CUserCmd cmd;
    cmd.viewangles.y = 10.0f;
    cmd.forwardmove = 120.0f;
    cmd.sidemove = -35.0f;
    cmd.upmove = 42.0f;

    H::AimUtils->FixMovement(&cmd, Vec3{ 0.0f, 100.0f, 0.0f });

    EXPECT_NEAR(cmd.forwardmove, 35.0f, 1e-4f);
    EXPECT_NEAR(cmd.sidemove, 120.0f, 1e-4f);
    EXPECT_FLOAT_EQ(cmd.upmove, 42.0f);
}

TEST(AimbotAimAssistContracts, MovementFixAvoidsPolarRoundTrip) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kAimUtilsSource);
    const auto functionStart = source.find("void CAimUtils::FixMovement");
    const auto functionEnd = source.find("bool CAimUtils::IsWeaponCapableOfHeadshot", functionStart);

    ASSERT_NE(functionStart, std::string::npos);
    ASSERT_NE(functionEnd, std::string::npos);

    const auto functionBody = source.substr(functionStart, functionEnd - functionStart);
    EXPECT_NE(functionBody.find("Math::SinCos"), std::string::npos);
    EXPECT_NE(functionBody.find("flCos * flForwardMove - flSin * flSideMove"), std::string::npos);
    EXPECT_EQ(functionBody.find("Math::VectorAngles"), std::string::npos);
    EXPECT_EQ(functionBody.find("Math::FastSqrt"), std::string::npos);
}
