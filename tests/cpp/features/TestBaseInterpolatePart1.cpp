#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseEntity_BaseInterpolatePart1.cpp";
}

// Source-contract test for BaseInterpolatePart1: must disable interpolation on doors
// (when accuracy improvements is on) and on the local player while shifting recharge.
TEST(BaseInterpolatePart1Contracts, HookFileExists) {
    const auto root = testhelpers::FindRepoRoot();
    const auto path = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(path));

    const auto src = testhelpers::ReadTextFile(path);
    EXPECT_NE(src.find("MAKE_HOOK(CBaseEntity_BaseInterpolatePart1"), std::string::npos);
}

TEST(BaseInterpolatePart1Contracts, HandlesShiftingAndDoors) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // Local player + Shifting::bRecharging path.
    EXPECT_NE(src.find("Shifting::bRecharging"), std::string::npos);

    // Door interpolation suppression (only when accuracy improvements is on).
    EXPECT_NE(src.find("CFG::Misc_Accuracy_Improvements"), std::string::npos);
    EXPECT_NE(src.find("ETFClassIds::CBaseDoor"), std::string::npos);
}

TEST(BaseInterpolatePart1Contracts, EarlyOutSetsNoMoreChanges) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // The disable-interp branch must mark bNoMoreChanges = 1 and short-circuit.
    EXPECT_NE(src.find("bNoMoreChanges = 1"), std::string::npos);
    // Otherwise the original engine path runs.
    EXPECT_NE(src.find("CALL_ORIGINAL(ecx, currentTime, oldOrigin, oldAngles, oldVel, bNoMoreChanges)"),
              std::string::npos);
}
