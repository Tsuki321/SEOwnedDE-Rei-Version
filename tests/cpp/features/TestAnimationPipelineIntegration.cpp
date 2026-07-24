#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFrameStage =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_FrameStageNotify.cpp";
constexpr const char* kUpdateAnimation =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CTFPlayer_UpdateClientSideAnimation.cpp";
constexpr const char* kRunCommand =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CPrediction_RunCommand.cpp";
constexpr const char* kSetupBones =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseAnimating_SetupBones.cpp";
constexpr const char* kAddVar =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_AddVar.cpp";
constexpr const char* kInterpolate =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_InterpolateServerEntities.cpp";
constexpr const char* kResetLatched =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_ResetLatched.cpp";
constexpr const char* kLagRecords =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp";
constexpr const char* kLagRecordsHeader =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.h";
}

TEST(AnimationPipelineIntegration, AllParticipatingFilesExist) {
    const auto root = testhelpers::FindRepoRoot();
    const char* files[] = {
        kFrameStage, kUpdateAnimation, kRunCommand, kSetupBones,
        kAddVar, kInterpolate, kResetLatched, kLagRecords,
    };

    for (const auto* file : files)
        EXPECT_TRUE(std::filesystem::exists(root / file));
}

TEST(AnimationPipelineIntegration, LiveAnimationRemainsEngineOwned) {
    const auto root = testhelpers::FindRepoRoot();
    const auto frame = testhelpers::ReadTextFile(root / kFrameStage);
    const auto update = testhelpers::ReadTextFile(root / kUpdateAnimation);
    const auto prediction = testhelpers::ReadTextFile(root / kRunCommand);

    EXPECT_EQ(frame.find("UpdateClientSideAnimation()"), std::string::npos);
    EXPECT_EQ(frame.find("G::bUpdatingAnims"), std::string::npos);
    EXPECT_EQ(update.find("G::bUpdatingAnims"), std::string::npos);
    EXPECT_NE(update.find("CALL_ORIGINAL(ecx);"), std::string::npos);
    EXPECT_EQ(prediction.find("FrameAdvance"), std::string::npos);
    EXPECT_EQ(prediction.find("pAnimState->Update"), std::string::npos);
}

TEST(AnimationPipelineIntegration, LiveInterpolationRemainsEngineOwned) {
    const auto root = testhelpers::FindRepoRoot();
    const auto addVar = testhelpers::ReadTextFile(root / kAddVar);
    const auto interpolate = testhelpers::ReadTextFile(root / kInterpolate);
    const auto reset = testhelpers::ReadTextFile(root / kResetLatched);

    EXPECT_EQ(addVar.find("m_iv_vecVelocity"), std::string::npos);
    EXPECT_EQ(addVar.find("m_iv_flMaxGroundSpeed"), std::string::npos);
    EXPECT_EQ(interpolate.find("cl_extrapolate"), std::string::npos);
    EXPECT_EQ(reset.find("Misc_Pred_Error_Jitter_Fix"), std::string::npos);
    EXPECT_NE(reset.find("CALL_ORIGINAL(ecx);"), std::string::npos);
}

TEST(AnimationPipelineIntegration, HistoricalBonesAreIsolatedFromLiveRendering) {
    const auto root = testhelpers::FindRepoRoot();
    const auto setup = testhelpers::ReadTextFile(root / kSetupBones);
    const auto lag = testhelpers::ReadTextFile(root / kLagRecords);

    EXPECT_NE(setup.find("CopyActiveBones"), std::string::npos);
    EXPECT_EQ(setup.find("F::LagRecords->GetRecord"), std::string::npos);
    EXPECT_NE(lag.find("pPlayer->SetupBones("), std::string::npos);
    EXPECT_NE(lag.find("AddRenderRecord"), std::string::npos);
    EXPECT_NE(lag.find("I::GlobalVars->curtime"), std::string::npos);
    EXPECT_EQ(lag.find("pPlayer->InvalidateBoneCache();"), std::string::npos);
}

TEST(AnimationPipelineIntegration, AimbotTickRemapUsesInterpAmount) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hitscan = testhelpers::ReadTextFile(
        root / "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp");
    const auto lagHeader = testhelpers::ReadTextFile(root / kLagRecordsHeader);

    EXPECT_NE(hitscan.find("CLagRecords::GetCommandTick"), std::string::npos);
    EXPECT_NE(hitscan.find("SimulationTime"), std::string::npos);
    EXPECT_NE(lagHeader.find("TIME_TO_TICKS(flPoseTime + SDKUtils::GetLerp())"), std::string::npos);
}
