#include <gtest/gtest.h>

#include "SDK/Helpers/AimUtils/AimUtils.h"

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHitscanSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp";
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
