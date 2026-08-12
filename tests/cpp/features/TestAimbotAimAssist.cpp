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

TEST(AimbotAimAssistContracts, ManualShotSelectsNewestHitAndPreservesTickOnMiss) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kHitscanSource);
    const auto functionStart = source.find("bool CAimbotHitscan::ResolveManualShot");
    const auto functionEnd = source.find("bool CAimbotHitscan::GetTarget", functionStart);

    ASSERT_NE(functionStart, std::string::npos);
    ASSERT_NE(functionEnd, std::string::npos);

    const auto functionBody = source.substr(functionStart, functionEnd - functionStart);
    EXPECT_NE(functionBody.find("pRecord->SimulationTime <= pBestRecord->SimulationTime"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(functionBody, "pCmd->tick_count ="), 1u);

    const auto missGuard = functionBody.find("if (!pBestRecord || !pBestPlayer)");
    const auto missReturn = functionBody.find("return false;", missGuard);
    const auto tickWrite = functionBody.find("pCmd->tick_count =");
    ASSERT_NE(missGuard, std::string::npos);
    ASSERT_NE(missReturn, std::string::npos);
    ASSERT_NE(tickWrite, std::string::npos);
    EXPECT_LT(missGuard, missReturn);
    EXPECT_LT(missReturn, tickWrite);

    const auto runStart = source.find("void CAimbotHitscan::Run");
    ASSERT_NE(runStart, std::string::npos);
    const auto runBody = source.substr(runStart);
    // Manual ownership is captured separately from auto aim-assist firing.
    EXPECT_NE(runBody.find("const bool bManualFiring = IsFiring(pCmd, pWeapon)"), std::string::npos);
    EXPECT_NE(runBody.find("G::bManualHitscanFiring = bManualFiring"), std::string::npos);
    EXPECT_NE(runBody.find("const bool bIsFiring = IsFiring(pCmd, pWeapon)"), std::string::npos);
    // No-target manual path is distinct from the aim-key / auto-fire branch.
    EXPECT_NE(runBody.find("else if (bManualFiring)"), std::string::npos);
    EXPECT_GE(testhelpers::CountOccurrences(runBody, "ResolveManualShot(pCmd, pLocal)"), 1u);
    // Aim assist only mutates angles when the aim key is held, not on pure manual fire.
    EXPECT_NE(runBody.find("if (aimKeyDown)"), std::string::npos);
    EXPECT_NE(runBody.find("Aim(pCmd, pLocal, target.AngleTo)"), std::string::npos);
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
