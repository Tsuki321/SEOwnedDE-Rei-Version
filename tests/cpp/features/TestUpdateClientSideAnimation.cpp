#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CTFPlayer_UpdateClientSideAnimation.cpp";
}

// Source-contract test for the local-player anim suppression hook. The hook is the
// pivot for switching between engine-driven local animation and our manual
// FrameAdvance/Update path in CPrediction_RunCommand.
TEST(UpdateClientSideAnimationContracts, HookFileExists) {
    const auto root = testhelpers::FindRepoRoot();
    const auto path = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(path));

    const auto src = testhelpers::ReadTextFile(path);
    EXPECT_NE(src.find("MAKE_HOOK(CTFPlayer_UpdateClientSideAnimation"), std::string::npos);
}

TEST(UpdateClientSideAnimationContracts, SuppressesLocalDefaultPath) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("CFG::Misc_Accuracy_Improvements"), std::string::npos);
    EXPECT_NE(src.find("ecx == pLocal"), std::string::npos);

    // Replacement path: viewmodel addons get refreshed in lieu of the default work.
    EXPECT_NE(src.find("UpdateAllViewmodelAddons()"), std::string::npos);

    // Halloween kart skips the suppression and runs the original engine path.
    EXPECT_NE(src.find("TF_COND_HALLOWEEN_KART"), std::string::npos);
}

TEST(UpdateClientSideAnimationContracts, RemoteGatedByUpdatingAnimsFlag) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // For non-local entities, original is invoked only when our FrameStage driver
    // sets G::bUpdatingAnims, ensuring deterministic per-tick playback.
    EXPECT_NE(src.find("G::bUpdatingAnims"), std::string::npos);
    EXPECT_NE(src.find("CALL_ORIGINAL(ecx);"), std::string::npos);
}
