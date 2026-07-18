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
    EXPECT_NE(src.find("ent != pLocal"), std::string::npos);
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

// Move-child entities (wearables, weapons, cosmetics) must not be served the
// root player's cached body bones. They need the engine path so attachments stay
// aligned with the rendered player instead of drifting ahead/behind the body.
TEST(SetupBonesContracts, BypassesMoveChildrenAndWearables) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("baseEnt != ent"), std::string::npos);
    EXPECT_NE(src.find("Move-child entities such as cosmetics and weapons"), std::string::npos);
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
    EXPECT_NE(src.find("flLiveYaw - pRecord->AbsAngles.y"), std::string::npos);
    // Sin/Cos must be computed once per frame, not per bone.
    EXPECT_NE(src.find("Math::SinCos(DEG2RAD(deltaYaw)"), std::string::npos);
}

// Phase 2: when two consecutive lag records exist, translation-blend toward the
// older record so per-tick pose snaps soften.
TEST(SetupBonesContracts, BlendsTranslationAcrossTwoRecords) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("nRecords >= 2"), std::string::npos);
    EXPECT_NE(src.find("F::LagRecords->GetRecord(pPlayer, 1)"), std::string::npos);
    EXPECT_NE(src.find("pRecord->SimulationTime - pPrev->SimulationTime"), std::string::npos);
    EXPECT_NE(src.find("std::clamp"), std::string::npos);
    // Only the position columns of each cached bone are blended; rotation
    // columns keep the freshest record so the live yaw correction is preserved.
    EXPECT_NE(src.find("cache.BoneData[i][0][3]"), std::string::npos);
    EXPECT_NE(src.find("cache.BoneData[i][2][3]"), std::string::npos);
}

TEST(SetupBonesContracts, AdjustedMatricesUseConservativeFrameLocalCacheKeys) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("g_AdjustedBoneCache"), std::string::npos);
    EXPECT_NE(src.find("cache.Frame == frame"), std::string::npos);
    EXPECT_NE(src.find("cache.SourceBones == sourceBones"), std::string::npos);
    EXPECT_NE(src.find("cache.RecordSimulationTime == record->SimulationTime"), std::string::npos);
    EXPECT_NE(src.find("SameVector(cache.LiveOrigin, liveOrigin)"), std::string::npos);
    EXPECT_NE(src.find("cache.LiveYaw == liveYaw"), std::string::npos);
    EXPECT_NE(src.find("cache.CurrentTime == currentTime"), std::string::npos);
    EXPECT_NE(src.find("cache.BoneMask == boneMask"), std::string::npos);
    EXPECT_NE(src.find("std::memcpy(pBoneToWorldOut, cache.BoneData.data()"), std::string::npos);
}

TEST(SetupBonesContracts, NullOutputFallsThroughToEngine) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    const auto nullGuard = src.find("if (!pBoneToWorldOut)");
    ASSERT_NE(nullGuard, std::string::npos);

    const auto bonesLookup = src.find("const auto bones", nullGuard);
    ASSERT_NE(bonesLookup, std::string::npos);

    const auto nullBranch = src.substr(nullGuard, bonesLookup - nullGuard);
    EXPECT_NE(nullBranch.find("return CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);"),
              std::string::npos);
    EXPECT_EQ(nullBranch.find("return true"), std::string::npos);
}

// Phase 2: failed-wearable bypass. If a child SetupBones failed during
// LagRecord capture, the cache short-circuit must defer to engine. Because
// the move-child early-return at the top of the hook guarantees baseEnt ==
// ent by the time we reach this check, a single HasFailedBones call covers
// both keys.
TEST(SetupBonesContracts, BypassesCacheForFailedChildBones) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("F::LagRecords->HasFailedBones(baseEnt)"), std::string::npos);
    EXPECT_EQ(src.find("F::LagRecords->HasFailedBones(ent)"), std::string::npos)
        << "baseEnt == ent at this point; a second HasFailedBones(ent) call "
           "is a redundant hash query.";
}

// The two-record blend must not run across a teleport: record 0 (post-jump) and
// record 1 (pre-jump) are far apart, so blending smears the skeleton across the
// gap and throws off a manual snap taken exactly on the teleport.
TEST(SetupBonesContracts, BlendSkipsAcrossTeleport) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("!pRecord->bTeleported"), std::string::npos);
}

// The blend interpolates LIMB pose only: the per-record origin step (O1 - O0)
// baked into the absolute bone matrices is subtracted so the blend never drags
// a moving target's limbs backward off the origin-corrected body (a manual-
// accuracy loss that scales with target speed).
TEST(SetupBonesContracts, BlendInterpolatesLimbPoseNotBodyPosition) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("pPrev->AbsOrigin.x - pRecord->AbsOrigin.x"), std::string::npos);
    EXPECT_NE(src.find(") - odx"), std::string::npos);
    EXPECT_NE(src.find(") - ody"), std::string::npos);
    EXPECT_NE(src.find(") - odz"), std::string::npos);
}
