#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CBaseAnimating_SetupBones.cpp";
constexpr const char* kLagHeader =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.h";
constexpr const char* kLagSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp";
}

TEST(SetupBonesContracts, HookExistsAndFallsThroughToEngine) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("MAKE_HOOK(CBaseAnimating_SetupBones"), std::string::npos);
    EXPECT_NE(src.find("CALL_ORIGINAL(ecx, pBoneToWorldOut, nMaxBones, boneMask, currentTime)"),
              std::string::npos);
}

TEST(SetupBonesContracts, LiveRenderingDoesNotReadLagRecordRing) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_EQ(src.find("CFG::Misc_SetupBones_Optimization"), std::string::npos);
    EXPECT_EQ(src.find("F::LagRecords->HasRecords"), std::string::npos);
    EXPECT_EQ(src.find("F::LagRecords->GetRecord"), std::string::npos);
    EXPECT_EQ(src.find("g_AdjustedBoneCache"), std::string::npos);
    EXPECT_EQ(src.find("NormalizeYawDelta"), std::string::npos);
}

TEST(SetupBonesContracts, HistoricalBypassRequiresExplicitScope) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hook = testhelpers::ReadTextFile(root / kHookSource);
    const auto header = testhelpers::ReadTextFile(root / kLagHeader);
    const auto lag = testhelpers::ReadTextFile(root / kLagSource);

    EXPECT_NE(hook.find("F::LagRecordMatrixHelper->IsActive()"), std::string::npos);
    EXPECT_NE(hook.find("F::LagRecordMatrixHelper->IsActiveFor(pEntity)"), std::string::npos);
    EXPECT_NE(hook.find("F::LagRecordMatrixHelper->CopyActiveBones"), std::string::npos);
    EXPECT_NE(header.find("class CLagRecordScope"), std::string::npos);
    EXPECT_NE(header.find("CopyActiveBones"), std::string::npos);
    EXPECT_NE(lag.find("entry.Player != pEntity"), std::string::npos);
    EXPECT_NE(lag.find("m_nActiveDepth - 1"), std::string::npos);
}

TEST(SetupBonesContracts, NullOutputBypassIsHistoricalOnly) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHookSource);

    EXPECT_NE(src.find("if (F::LagRecordMatrixHelper->IsActive() && ecx)"), std::string::npos);
    EXPECT_NE(src.find("if (!pBoneToWorldOut)"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(src, "return true;"), 2u);
    EXPECT_EQ(testhelpers::CountOccurrences(src, "CALL_ORIGINAL"), 1u);
}
