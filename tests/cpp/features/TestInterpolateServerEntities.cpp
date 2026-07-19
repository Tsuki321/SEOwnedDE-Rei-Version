#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_InterpolateServerEntities.cpp";
}

TEST(InterpolateServerEntitiesContracts, HookDefersToEngine) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("MAKE_HOOK(CBaseEntity_InterpolateServerEntities"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(src, "CALL_ORIGINAL(ecx);"), 1u);
}

TEST(InterpolateServerEntitiesContracts, DoesNotOverrideExtrapolation) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_EQ(src.find("cl_extrapolate"), std::string::npos);
    EXPECT_EQ(src.find("SetValue"), std::string::npos);
    EXPECT_EQ(src.find("Misc_Accuracy_Improvements"), std::string::npos);
}
