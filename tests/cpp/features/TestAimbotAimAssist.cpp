#include <gtest/gtest.h>

#include "SDK/Helpers/AimUtils/AimUtils.h"

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHitscanSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp";
constexpr const char* kHitscanHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.h";
constexpr const char* kAimbotSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/Aimbot.cpp";
constexpr const char* kCreateMoveSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/ClientModeShared_CreateMove.cpp";
constexpr const char* kCfgHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/CFG.h";
constexpr const char* kAimUtilsSource = "SEOwnedDE/SEOwnedDE/src/SDK/Helpers/AimUtils/AimUtils.cpp";
}

TEST(AimbotAimAssistContracts, HitscanSourceContainsAimAssistBranch) {
    const auto root = testhelpers::FindRepoRoot();
    const auto sourcePath = root / kHitscanSource;

    ASSERT_TRUE(std::filesystem::exists(sourcePath));

    const auto source = testhelpers::ReadTextFile(sourcePath);
    EXPECT_NE(source.find("case 3:"), std::string::npos);
    EXPECT_NE(source.find("Aimbot_Hitscan_AimAssist_Strength"), std::string::npos);
    EXPECT_NE(source.find("Aimbot_Hitscan_AimAssist_Stabilization"), std::string::npos);
    EXPECT_NE(source.find("if (!CFG::Aimbot_Hitscan_AimAssist_Stabilization)"), std::string::npos);
    EXPECT_NE(source.find("pCmd->viewangles += vDelta / CFG::Aimbot_Hitscan_AimAssist_Strength"), std::string::npos);
    EXPECT_NE(source.find("Vec3 vAssistStep = vDelta / CFG::Aimbot_Hitscan_AimAssist_Strength"), std::string::npos);
    EXPECT_NE(source.find("vAssistStep.LengthSqr()"), std::string::npos);
    EXPECT_NE(source.find("flMaxAssistStep"), std::string::npos);
}

TEST(AimbotAimAssistContracts, CfgDeclaresAimAssistTuningVar) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cfgPath = root / kCfgHeader;

    ASSERT_TRUE(std::filesystem::exists(cfgPath));

    const auto cfgText = testhelpers::ReadTextFile(cfgPath);
    EXPECT_NE(cfgText.find("CFGVAR(Aimbot_Hitscan_AimAssist_Strength"), std::string::npos);
    EXPECT_NE(cfgText.find("CFGVAR(Aimbot_Hitscan_AimAssist_Stabilization"), std::string::npos);
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
    EXPECT_NE(functionBody.find("pCmd->viewangles + pLocal->m_vecPunchAngle()"), std::string::npos);
    EXPECT_NE(functionBody.find("EEntGroup::PLAYERS_ENEMIES"), std::string::npos);
    EXPECT_NE(functionBody.find("CLagRecords::IsRecordUsable"), std::string::npos);
    EXPECT_NE(functionBody.find("CLagRecordScope scope(pRecord)"), std::string::npos);
    EXPECT_NE(functionBody.find("scope.IsActive()"), std::string::npos);
    EXPECT_NE(functionBody.find("H::AimUtils->TraceEntityBullet"), std::string::npos);
    EXPECT_NE(functionBody.find("HistoricalPoseMayIntersectRay"), std::string::npos);
    EXPECT_EQ(functionBody.find("Aimbot_Hitscan_FOV"), std::string::npos);
}

TEST(AimbotAimAssistContracts, ManualShotStampsLivePoseFirstThenHistoricalRecords) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kHitscanSource);
    const auto functionStart = source.find("bool CAimbotHitscan::ResolveManualShot");
    const auto functionEnd = source.find("bool CAimbotHitscan::ValidateTarget", functionStart);

    ASSERT_NE(functionStart, std::string::npos);
    ASSERT_NE(functionEnd, std::string::npos);

    const auto functionBody = source.substr(functionStart, functionEnd - functionStart);
    EXPECT_EQ(functionBody.find("pRecord->SimulationTime <= pBestRecord->SimulationTime"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(functionBody, "pCmd->tick_count ="), 2u);

    // The live pose outranks every record: the crosshair is on it, so rewinding
    // past it to a stale (or foreign) record moved the real target off the shot
    // server-side. The live branch runs first and is accuracy-gated; records
    // are walked only when no live hitbox lies on the ray.
    const auto liveStamp = functionBody.find("CLagRecords::GetCommandTick(pPlayer->m_flSimulationTime())");
    const auto accuracyGate = functionBody.find("CFG::Misc_Accuracy_Improvements");
    const auto historicalStamp = functionBody.find("CLagRecords::GetCommandTick(pBestRecord->SimulationTime)");
    ASSERT_NE(liveStamp, std::string::npos);
    ASSERT_NE(accuracyGate, std::string::npos);
    ASSERT_NE(historicalStamp, std::string::npos);
    EXPECT_LT(accuracyGate, liveStamp);
    EXPECT_LT(liveStamp, historicalStamp);

    // The live scan is not gated on the backtrack toggle: it replaces the old
    // accuracy-gated fallback that ran after the records, so it must run even
    // with manual backtrack disabled.
    const auto liveScan = functionBody.find("H::AimUtils->TraceEntityBullet");
    const auto backtrackGate = functionBody.find("Aimbot_Hitscan_Manual_Backtrack");
    ASSERT_NE(liveScan, std::string::npos);
    ASSERT_NE(backtrackGate, std::string::npos);
    EXPECT_LT(liveScan, backtrackGate);

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
    // Aim assist only mutates angles when the aim key is held, not on pure manual fire.
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
