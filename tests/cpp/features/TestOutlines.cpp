#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Outlines";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Outlines/Outlines.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Outlines/Outlines.h";
}

TEST(OutlinesContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(OutlinesContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    const auto headerSource = testhelpers::ReadTextFile(root / kHeaderSource);
    EXPECT_NE(mainSource.find("CFG::Outlines_Active"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Draw"), std::string::npos);
    EXPECT_NE(mainSource.find("COutlines::Initialize("), std::string::npos);
    EXPECT_NE(mainSource.find("COutlines::MarkDrawn("), std::string::npos);
    EXPECT_NE(headerSource.find("m_arrDrawnGenerations"), std::string::npos);
    EXPECT_NE(headerSource.find("m_arrDrawnHandles"), std::string::npos);
    EXPECT_NE(headerSource.find("m_nDrawFrame"), std::string::npos);
    EXPECT_NE(headerSource.find("return pMaterial && (pMaterial == m_pMatGlowColor"), std::string::npos);
    EXPECT_NE(mainSource.find("bCreateBloomResources && !m_pRenderBuffer1"), std::string::npos);
    EXPECT_NE(mainSource.find("CRenderContextScope renderContext"), std::string::npos);
    EXPECT_NE(mainSource.find("GetModelPlayerCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("GetModelBuildingCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("GetModelProjectileCandidates("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(OutlinesContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(13));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(1));
}
