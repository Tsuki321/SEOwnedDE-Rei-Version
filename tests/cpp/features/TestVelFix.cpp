#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_SetAbsVelocity.cpp";
}

// Source-contract test for VelFix (CBaseEntity_SetAbsVelocity hook).
//
// VelFix corrects the vertical velocity component when a player crouches or uncrouches
// mid-air. Without it, the engine reports stale vz across duck transitions, which leads
// to jitter in MovementSimulation and aimbot prediction.
TEST(VelFixContracts, HookFileExistsAndHasExpectedShape) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hookPath = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(hookPath))
        << "Expected VelFix hook at " << hookPath.string();

    const auto src = testhelpers::ReadTextFile(hookPath);

    // Hook is registered for the SetAbsVelocity virtual.
    EXPECT_NE(src.find("MAKE_HOOK(CBaseEntity_SetAbsVelocity"), std::string::npos);

    // Return-address gating ensures we only correct calls coming from PostDataUpdate.
    EXPECT_NE(src.find("CBasePlayer_PostDataUpdate_SetAbsVelocityCall"), std::string::npos);
    EXPECT_NE(src.find("_ReturnAddress()"), std::string::npos);
}

TEST(VelFixContracts, ChecksDuckTransitionBranches) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // Both duck-down and duck-up transitions must be handled with opposite z offsets.
    EXPECT_NE(src.find("FL_DUCKING"), std::string::npos);
    EXPECT_NE(src.find("FL_ONGROUND"), std::string::npos);
    EXPECT_NE(src.find("z += 20.0f"), std::string::npos);
    EXPECT_NE(src.find("z -= 20.0f"), std::string::npos);

    // Air-only correction: both current and old flags must lack FL_ONGROUND.
    EXPECT_GE(testhelpers::CountOccurrences(src, "& FL_ONGROUND"), 2u);
}

TEST(VelFixContracts, GuardsZeroSimTimeDelta) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // Avoid divide-by-zero / negative time deltas.
    EXPECT_NE(src.find("flSimTimeDelta > 0.0f"), std::string::npos);

    // Vertical velocity is rebuilt from the corrected origin delta.
    EXPECT_NE(src.find("vNewVelocity.z"), std::string::npos);
    EXPECT_NE(src.find("/ flSimTimeDelta"), std::string::npos);
}

TEST(VelFixContracts, FallsThroughToOriginalOnNoRecord) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // Final fallback path must invoke the original.
    EXPECT_NE(src.find("CALL_ORIGINAL(ecx, vecAbsVelocity);"), std::string::npos);
    EXPECT_GE(testhelpers::CountOccurrences(src, "CALL_ORIGINAL"), 2u);
}
