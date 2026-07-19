#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_FrameStageNotify.cpp";
constexpr const char* kLagRecordsSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp";
}

TEST(FrameStageAnimDriveContracts, HookFileExistsAndIsRegistered) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("MAKE_HOOK(IBaseClientDLL_FrameStageNotify"), std::string::npos);
    EXPECT_NE(src.find("Memory::GetVFunc(I::BaseClientDLL, 35)"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, DoesNotManuallyDriveAnimations) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_EQ(src.find("UpdateClientSideAnimation()"), std::string::npos);
    EXPECT_EQ(src.find("G::bUpdatingAnims"), std::string::npos);
    EXPECT_EQ(src.find("TICK_INTERVAL"), std::string::npos);
    EXPECT_EQ(src.find("nDifference"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, CapturesCoherentRenderedEnemiesOnly) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("GetGroup(EEntGroup::PLAYERS_ENEMIES)"), std::string::npos);
    EXPECT_NE(src.find("I::GlobalVars->curtime - SDKUtils::GetLerp()"), std::string::npos);
    EXPECT_NE(src.find("flPoseTime > pPlayer->m_flSimulationTime()"), std::string::npos);
    EXPECT_NE(src.find("CLagRecords::ShouldCaptureRecord(pLocal, pPlayer)"), std::string::npos);
    EXPECT_NE(src.find("F::LagRecords->AddRenderRecord(pPlayer, flPoseTime)"), std::string::npos);
    EXPECT_EQ(src.find("CFG::Misc_SetupBones_Optimization"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, CapturePolicyPrecedesBoneCapture) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hookSrc = testhelpers::ReadTextFile(root / kHookSource);
    const auto lagSrc = testhelpers::ReadTextFile(root / kLagRecordsSource);

    const auto policyPos = hookSrc.find("CLagRecords::ShouldCaptureRecord(pLocal, pPlayer)");
    const auto capturePos = hookSrc.find("F::LagRecords->AddRenderRecord(pPlayer, flPoseTime)");
    ASSERT_NE(policyPos, std::string::npos);
    ASSERT_NE(capturePos, std::string::npos);
    EXPECT_LT(policyPos, capturePos);

    EXPECT_NE(lagSrc.find("if (!AreConsumersActive())"), std::string::npos);
    EXPECT_NE(lagSrc.find("pPlayer->m_iTeamNum() == pLocal->m_iTeamNum()"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, MaintainsMovementAndVelocityRecords) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("F::MovementSimulation->StoreMoveRecord(pPlayer)"), std::string::npos);
    EXPECT_NE(src.find("F::LagRecords->UpdateRecords()"), std::string::npos);
    EXPECT_NE(src.find("G::arrVelFixRecords"), std::string::npos);
    EXPECT_NE(src.find("nIndex > MAX_PLAYERS"), std::string::npos);
}
