#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CTFPlayer_UpdateClientSideAnimation.cpp";
}

TEST(UpdateClientSideAnimationContracts, HookAlwaysDefersToEngine) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("MAKE_HOOK(CTFPlayer_UpdateClientSideAnimation"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(src, "CALL_ORIGINAL(ecx);"), 1u);
}

TEST(UpdateClientSideAnimationContracts, DoesNotGateLiveAnimation) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_EQ(src.find("G::bUpdatingAnims"), std::string::npos);
    EXPECT_EQ(src.find("Misc_Accuracy_Improvements"), std::string::npos);
    EXPECT_EQ(src.find("UpdateAllViewmodelAddons"), std::string::npos);
}
