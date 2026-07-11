#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_FrameStageNotify.cpp";
constexpr const char* kLagRecordsSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp";
}

// FrameStageNotify is the per-frame driver that catches up remote-player animation
// state across simulation-time deltas, captures lag records, and maintains the
// VelFix scratchpad. Because it depends on engine-side singletons we validate it
// via source contract.

TEST(FrameStageAnimDriveContracts, HookFileExistsAndIsRegistered) {
    const auto root = testhelpers::FindRepoRoot();
    const auto path = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(path));

    const auto src = testhelpers::ReadTextFile(path);
    EXPECT_NE(src.find("MAKE_HOOK(IBaseClientDLL_FrameStageNotify"), std::string::npos);
    EXPECT_NE(src.find("Memory::GetVFunc(I::BaseClientDLL, 35)"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, ClampsSimTimeDeltaToSafeRange) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // The clamp [0, 22] caps animation catch-up cost on packet bursts.
    EXPECT_NE(src.find("std::clamp(TIME_TO_TICKS"), std::string::npos);
    EXPECT_NE(src.find(", 0, 22)"), std::string::npos);

    // Sim time delta uses old vs current values.
    EXPECT_NE(src.find("m_flSimulationTime() - pPlayer->m_flOldSimulationTime()"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, DrivesUpdateClientSideAnimationLoop) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("CFG::Misc_Accuracy_Improvements"), std::string::npos);
    EXPECT_NE(src.find("G::bUpdatingAnims = true"), std::string::npos);
    EXPECT_NE(src.find("G::bUpdatingAnims = false"), std::string::npos);
    EXPECT_NE(src.find("pPlayer->UpdateClientSideAnimation()"), std::string::npos);

    // Frametime is locked to TICK_INTERVAL during the catch-up loop.
    EXPECT_NE(src.find("TICK_INTERVAL"), std::string::npos);
    EXPECT_NE(src.find("I::Prediction->m_bEnginePaused"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, MaintainsLagRecordsAndVelFix) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("F::LagRecords->AddRecord(pPlayer)"), std::string::npos);
    EXPECT_NE(src.find("F::LagRecords->UpdateRecords()"), std::string::npos);

    // VelFix scratchpad is a fixed per-entindex array, refreshed every net update
    // and bounded by construction (no runtime size cap needed).
    EXPECT_NE(src.find("G::arrVelFixRecords"), std::string::npos);
    EXPECT_NE(src.find("nIndex > MAX_PLAYERS"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, GatedBySetupBonesOptimizationForLagRecords) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // SetupBones optimization expands lag records to ALL players (incl. teammates),
    // otherwise only enemies are recorded.
    EXPECT_NE(src.find("CFG::Misc_SetupBones_Optimization"), std::string::npos);
    EXPECT_NE(src.find("m_iTeamNum() != pLocal->m_iTeamNum()"), std::string::npos);
}

TEST(FrameStageAnimDriveContracts, UsesCentralizedLagRecordCapturePolicy) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hookSrc = testhelpers::ReadTextFile(root / kHookSource);
    const auto lagSrc = testhelpers::ReadTextFile(root / kLagRecordsSource);

    // Hook delegates capture gating to CLagRecords (no duplicated CFG OR-list).
    EXPECT_NE(hookSrc.find("CLagRecords::ShouldCaptureRecord(pLocal, pPlayer)"), std::string::npos);
    EXPECT_EQ(hookSrc.find("CFG::Misc_LagRecords_Skip_Offscreen"), std::string::npos);
    EXPECT_EQ(hookSrc.find("CFG::Aimbot_Hitscan_Target_LagRecords"), std::string::npos);

    EXPECT_NE(lagSrc.find("CLagRecords::AreConsumersActive()"), std::string::npos);
    EXPECT_NE(lagSrc.find("CLagRecords::ShouldCaptureRecord("), std::string::npos);
    EXPECT_NE(lagSrc.find("CFG::Misc_LagRecords_Skip_Offscreen"), std::string::npos);
}
