#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_InterpolateServerEntities.cpp";
}

// Source-contract: confirm cl_extrapolate is force-clamped to 0 when accuracy
// improvements are on, and that the hook always defers to the original.
TEST(InterpolateServerEntitiesContracts, HookFileExists) {
    const auto root = testhelpers::FindRepoRoot();
    const auto path = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(path));

    const auto src = testhelpers::ReadTextFile(path);
    EXPECT_NE(src.find("MAKE_HOOK(CBaseEntity_InterpolateServerEntities"), std::string::npos);
}

TEST(InterpolateServerEntitiesContracts, ForcesClExtrapolateToZero) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("CFG::Misc_Accuracy_Improvements"), std::string::npos);
    EXPECT_NE(src.find("\"cl_extrapolate\""), std::string::npos);
    EXPECT_NE(src.find("cl_extrapolate->SetValue(0)"), std::string::npos);
    // Re-clamping should be guarded by a non-zero check to avoid spurious churn.
    EXPECT_NE(src.find("cl_extrapolate->GetInt()"), std::string::npos);
}

TEST(InterpolateServerEntitiesContracts, AlwaysCallsOriginal) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("CALL_ORIGINAL(ecx);"), std::string::npos);
}
