#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CMultiPlayerAnimState_ResetGestureSlot.cpp";
}

// Source-contract test: when fake taunt is active, the local player's VCD gesture slot
// must NOT be reset, otherwise the fake taunt animation desyncs from the visual state.
TEST(MultiPlayerAnimStateResetGestureSlotContracts, HookFileExists) {
    const auto root = testhelpers::FindRepoRoot();
    const auto path = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(path));

    const auto src = testhelpers::ReadTextFile(path);
    EXPECT_NE(src.find("MAKE_HOOK(CMultiPlayerAnimState_ResetGestureSlot"), std::string::npos);
}

TEST(MultiPlayerAnimStateResetGestureSlotContracts, GuardsLocalVcdSlotDuringFakeTaunt) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("GESTURE_SLOT_VCD"), std::string::npos);
    EXPECT_NE(src.find("G::bStartedFakeTaunt"), std::string::npos);
    EXPECT_NE(src.find("m_pEntity == pLocal"), std::string::npos);
}

TEST(MultiPlayerAnimStateResetGestureSlotContracts, FallsThroughForOtherCases) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("CALL_ORIGINAL(ecx, iGestureSlot);"), std::string::npos);
}
