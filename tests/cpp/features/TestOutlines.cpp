#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Outlines";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Outlines/Outlines.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Outlines/Outlines.h";
constexpr const char* kViewModelHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CViewRender_DrawViewModels.cpp";
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
    EXPECT_NE(headerSource.find("m_nLastModelFrame"), std::string::npos);
    EXPECT_NE(headerSource.find("m_nLastCompositeFrame"), std::string::npos);
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

TEST(OutlinesContracts, ModelAndCompositePassesAreFrameLocal) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);
    const auto viewModelHookSource = testhelpers::ReadTextFile(root / kViewModelHookSource);

    EXPECT_NE(mainSource.find("m_nLastModelFrame == frame"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nLastModelFrame = frame"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nDrawFrame != frame || m_nLastCompositeFrame == frame"),
              std::string::npos);
    EXPECT_NE(mainSource.find("m_nLastCompositeFrame = frame"), std::string::npos);
	EXPECT_NE(viewModelHookSource.find("IsCurrentRenderViewMainWorld()"), std::string::npos);

    const auto readinessPos = viewModelHookSource.find(
        "bNeedsPreparedMainWorld && !bMainWorldPassComplete");
    const auto claimPos = viewModelHookSource.find(
        "RenderPassState::TryBeginCompositePass(frame)");
    ASSERT_NE(readinessPos, std::string::npos);
    ASSERT_NE(claimPos, std::string::npos);
    EXPECT_LT(readinessPos, claimPos)
        << "Auxiliary views must not consume the main frame's composite claim.";
}

TEST(OutlinesContracts, CleanupDropsLevelScopedEntityAndFrameState) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(mainSource.find("m_nLastModelFrame = -1"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nLastCompositeFrame = -1"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nDrawFrame = -1"), std::string::npos);
    EXPECT_NE(mainSource.find("m_vecOutlineEntities.clear()"), std::string::npos);
    EXPECT_NE(mainSource.find("RenderPassState::ResetFrameGates()"), std::string::npos);
    EXPECT_NE(mainSource.find("m_pBloomAmount = nullptr"), std::string::npos);
}
