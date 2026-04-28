#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CSequenceTransitioner_CheckForSequenceChange.cpp";
}

// Phase 2 contract: the sequence transitioner hook must NO LONGER force
// bInterpolate to false. The bone-fidelity work in CBaseAnimating_SetupBones
// (yaw delta correction + two-record translation lerp) makes cached bones
// path-aware, so transition blending is now safe to keep enabled.
//
// The hook file remains as scaffolding for future overrides without
// re-introducing a registration.

TEST(SequenceTransitionerContracts, HookFileExists) {
    const auto root = testhelpers::FindRepoRoot();
    const auto path = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(path));

    const auto src = testhelpers::ReadTextFile(path);
    EXPECT_NE(src.find("MAKE_HOOK(CSequenceTransitioner_CheckForSequenceChange"), std::string::npos);
}

TEST(SequenceTransitionerContracts, NoLongerForcesInterpolateFalse) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // The Phase 2 change removes this exact assignment; if it ever returns the
    // bone-fidelity contract is broken because slerp would fight transition blends.
    EXPECT_EQ(src.find("bInterpolate = false"), std::string::npos)
        << "Phase 2 expects sequence transition interp to remain enabled.";
}

TEST(SequenceTransitionerContracts, AlwaysCallsOriginalUnmodified) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // The hook body must still defer to the engine implementation with the
    // bInterpolate value passed in untouched.
    EXPECT_NE(src.find("CALL_ORIGINAL(ecx, hdr, nCurSequence, bForceNewSequence, bInterpolate)"),
              std::string::npos);
}
