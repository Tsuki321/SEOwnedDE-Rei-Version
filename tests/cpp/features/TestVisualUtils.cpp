#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/VisualUtils";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/VisualUtils/VisualUtils.cpp";
}

TEST(VisualUtilsContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(2));
}

TEST(VisualUtilsContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Color_Target"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Draw"), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::IsEntityOwnedBy("), std::string::npos);
    EXPECT_NE(mainSource.find("ResetFrameCacheIfNeeded("), std::string::npos);
    EXPECT_NE(mainSource.find("GetFrameCacheEntry("), std::string::npos);
    EXPECT_NE(mainSource.find("m_arrFrameCache"), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::ShouldRenderPlayer("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::ShouldRenderBuilding("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::ShouldRenderProjectile("), std::string::npos);
    EXPECT_NE(mainSource.find("IsOwnedByLocalCached("), std::string::npos);
    EXPECT_NE(mainSource.find("BuildPlayerCandidatesIfNeeded("), std::string::npos);
    EXPECT_NE(mainSource.find("BuildBuildingCandidatesIfNeeded("), std::string::npos);
    EXPECT_NE(mainSource.find("BuildProjectileCandidatesIfNeeded("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::GetPlayerCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::GetBuildingCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::GetProjectileCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("BuildModelPlayerCandidatesIfNeeded("), std::string::npos);
    EXPECT_NE(mainSource.find("BuildModelBuildingCandidatesIfNeeded("), std::string::npos);
    EXPECT_NE(mainSource.find("BuildModelProjectileCandidatesIfNeeded("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::GetModelPlayerCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::GetModelBuildingCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("CVisualUtils::GetModelProjectileCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("m_vecPlayerCandidates"), std::string::npos);
    EXPECT_NE(mainSource.find("m_vecModelPlayerCandidates"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nCachedScreenW"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nCachedScreenH"), std::string::npos);
    EXPECT_NE(mainSource.find("OwnedByLocalValid"), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(VisualUtilsContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(4));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(9));
}
