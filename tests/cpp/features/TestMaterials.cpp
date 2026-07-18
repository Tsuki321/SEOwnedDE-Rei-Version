#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Materials";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Materials/Materials.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Materials/Materials.h";
constexpr const char* kContextScopeSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/Rendering/RenderContextScope.h";
constexpr const char* kRenderPassStateSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/Rendering/RenderPassState.h";
constexpr const char* kWorldRenderHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CParticleSystemMgr_DrawRenderCache.cpp";
constexpr const char* kModelRenderHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/IVModelRender_DrawModelExecute.cpp";
}

TEST(MaterialsContracts, RenderContextScopeReleasesOwningReference) {
    const auto root = testhelpers::FindRepoRoot();
    const auto scopeSource = testhelpers::ReadTextFile(root / kContextScopeSource);

    EXPECT_NE(scopeSource.find("GetRenderContext()"), std::string::npos);
    EXPECT_NE(scopeSource.find("m_pContext->Release()"), std::string::npos);
    EXPECT_NE(scopeSource.find("CRenderContextScope(const CRenderContextScope&) = delete"), std::string::npos);
    EXPECT_EQ(scopeSource.find("BeginRender()"), std::string::npos);
    EXPECT_EQ(scopeSource.find("EndRender()"), std::string::npos);
}

TEST(MaterialsContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(MaterialsContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    const auto headerSource = testhelpers::ReadTextFile(root / kHeaderSource);
    EXPECT_NE(mainSource.find("CFG::Outlines_Players_Active"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Entities"), std::string::npos);
    EXPECT_NE(mainSource.find("CMaterials::Initialize("), std::string::npos);
    EXPECT_NE(mainSource.find("CMaterials::MarkDrawn("), std::string::npos);
    EXPECT_NE(headerSource.find("m_arrDrawnGenerations"), std::string::npos);
    EXPECT_NE(headerSource.find("m_arrDrawnHandles"), std::string::npos);
    EXPECT_NE(headerSource.find("m_nDrawFrame"), std::string::npos);
    EXPECT_NE(headerSource.find("return pMaterial && (pMaterial == m_pFlat"), std::string::npos);
    EXPECT_NE(mainSource.find("CMaterials::Run(IMatRenderContext* pRenderContext)"), std::string::npos);
    EXPECT_NE(mainSource.find("ApplyWorldColor"), std::string::npos);
    EXPECT_NE(mainSource.find("GetModelPlayerCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("GetModelBuildingCandidates("), std::string::npos);
    EXPECT_NE(mainSource.find("GetModelProjectileCandidates("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(MaterialsContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(22));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(5));
}

TEST(MaterialsContracts, PlayerAttachmentsUseOwningPlayerOutlineRules) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);
    const auto headerSource = testhelpers::ReadTextFile(root / kHeaderSource);

    EXPECT_NE(mainSource.find("pPlayerOwner || pEntity->GetClassId() == ETFClassIds::CTFPlayer"),
              std::string::npos);
    EXPECT_NE(mainSource.find("DrawEntity(pAttach, pRenderContext, pPlayer)"),
              std::string::npos);
    EXPECT_NE(headerSource.find("C_TFPlayer* pPlayerOwner = nullptr"),
              std::string::npos);
}

TEST(MaterialsContracts, DrawSuppressionIsScopedToMainWorldPass) {
    const auto root = testhelpers::FindRepoRoot();
    const auto passSource = testhelpers::ReadTextFile(root / kRenderPassStateSource);
    const auto worldHookSource = testhelpers::ReadTextFile(root / kWorldRenderHookSource);
    const auto modelHookSource = testhelpers::ReadTextFile(root / kModelRenderHookSource);

    EXPECT_NE(passSource.find("g_bDrawingMainWorld = m_bPrevious"), std::string::npos);
    EXPECT_NE(worldHookSource.find("CMainWorldScope mainWorldScope(viewID == VIEW_MAIN)"),
              std::string::npos);
    EXPECT_NE(modelHookSource.find("!bTakingScreenshot && RenderPassState::g_bDrawingMainWorld"),
              std::string::npos);
}
