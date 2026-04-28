#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseAnimating_SetupBones.cpp";
}

// Source-contract tests for the SetupBones cache + delta-correction logic.
// This is the hot path for non-local player skeleton retrieval and is gated by
// Misc_SetupBones_Optimization.
TEST(SetupBonesContracts, HookFileExistsAndIsHooked) {
    const auto root = testhelpers::FindRepoRoot();
    const auto path = root / kHookSource;

    ASSERT_TRUE(std::filesystem::exists(path));

    const auto src = testhelpers::ReadTextFile(path);
    EXPECT_NE(src.find("MAKE_HOOK(CBaseAnimating_SetupBones"), std::string::npos);
    EXPECT_NE(src.find("Signatures::CBaseAnimating_SetupBones.Get()"), std::string::npos);
}

TEST(SetupBonesContracts, GatedByOptimizationFlag) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("CFG::Misc_SetupBones_Optimization"), std::string::npos);
    // IsSettingUpBones gate prevents reentrancy with LagRecords::AddRecord.
    EXPECT_NE(src.find("IsSettingUpBones()"), std::string::npos);
}

TEST(SetupBonesContracts, OnlyAppliesToRemoteCTFPlayers) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("ETFClassIds::CTFPlayer"), std::string::npos);
    // Local player must be excluded.
    EXPECT_NE(src.find("ent != H::Entities->GetLocal()"), std::string::npos);
}

TEST(SetupBonesContracts, CopiesCachedBonesAndAppliesDelta) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // Cached bone fast-path: memcpy from GetCachedBoneData.
    EXPECT_NE(src.find("GetCachedBoneData()"), std::string::npos);
    EXPECT_NE(src.find("std::memcpy(pBoneToWorldOut"), std::string::npos);

    // Translation delta correction must reference the lag record's AbsOrigin.
    EXPECT_NE(src.find("pRecord->AbsOrigin"), std::string::npos);
    EXPECT_NE(src.find("GetAbsOrigin()"), std::string::npos);
}

TEST(SetupBonesContracts, FallsThroughForNonPlayersAndLocal) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    // Final fallback to engine implementation.
    EXPECT_NE(src.find("CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime)"),
              std::string::npos);
}

// Phase 2: yaw rotational delta correction must be present and gated on a
// non-trivial yaw difference. Without it, the cached pose snaps in translation
// only, leaving the skeleton mis-aimed during fast spins.
TEST(SetupBonesContracts, AppliesYawRotationalDelta) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("NormalizeYawDelta"), std::string::npos);
    EXPECT_NE(src.find("RotateBoneAroundOriginYaw"), std::string::npos);
    EXPECT_NE(src.find("ent->GetAbsAngles().y - pRecord->AbsAngles.y"), std::string::npos);
    // Sin/Cos must be computed once per frame, not per bone.
    EXPECT_NE(src.find("Math::SinCos(DEG2RAD(deltaYaw)"), std::string::npos);
}

// Phase 2: when two consecutive lag records exist, translation-blend toward the
// older record so per-tick pose snaps soften.
TEST(SetupBonesContracts, BlendsTranslationAcrossTwoRecords) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("nRecords >= 2"), std::string::npos);
    EXPECT_NE(src.find("F::LagRecords->GetRecord(pPlayer, 1, true)"), std::string::npos);
    EXPECT_NE(src.find("pRecord->SimulationTime - pPrev->SimulationTime"), std::string::npos);
    EXPECT_NE(src.find("std::clamp"), std::string::npos);
    // Only the position columns of each bone are blended; rotation columns keep
    // the freshest record so the live yaw correction above is preserved.
    EXPECT_NE(src.find("pBoneToWorldOut[i][0][3]"), std::string::npos);
    EXPECT_NE(src.find("pBoneToWorldOut[i][2][3]"), std::string::npos);
}

// Phase 2: failed-wearable bypass. If a child SetupBones failed during
// LagRecord capture, the cache short-circuit must defer to engine.
TEST(SetupBonesContracts, BypassesCacheForFailedChildBones) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("F::LagRecords->HasFailedBones(ent)"), std::string::npos);
}
