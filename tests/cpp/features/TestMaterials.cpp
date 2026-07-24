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
constexpr const char* kViewModelHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CViewRender_DrawViewModels.cpp";
constexpr const char* kRenderViewHookSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/CViewRender_RenderView.cpp";
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
    EXPECT_NE(headerSource.find("m_nLastRunFrame"), std::string::npos);
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

TEST(MaterialsContracts, MainWorldEffectsAreClaimedOncePerFrame) {
    const auto root = testhelpers::FindRepoRoot();
    const auto passSource = testhelpers::ReadTextFile(root / kRenderPassStateSource);
    const auto worldHookSource = testhelpers::ReadTextFile(root / kWorldRenderHookSource);
    const auto viewModelHookSource = testhelpers::ReadTextFile(root / kViewModelHookSource);

    EXPECT_NE(passSource.find("std::atomic<int>"), std::string::npos);
    EXPECT_NE(passSource.find("compare_exchange_weak"), std::string::npos);
    EXPECT_NE(passSource.find("TryBeginMainWorldModelPass"), std::string::npos);
    EXPECT_NE(passSource.find("CompleteMainWorldModelPass"), std::string::npos);
    EXPECT_NE(passSource.find("TryBeginCompositePass"), std::string::npos);

    const auto claimPos = worldHookSource.find(
        "bNeedsRenderPass && RenderPassState::TryBeginMainWorldModelPass(frame)");
    const auto contextPos = worldHookSource.find("CRenderContextScope renderContext");
    ASSERT_NE(claimPos, std::string::npos);
    ASSERT_NE(contextPos, std::string::npos);
    EXPECT_LT(claimPos, contextPos);

    EXPECT_NE(worldHookSource.find("RenderPassState::CompleteMainWorldModelPass(frame)"),
              std::string::npos);
    EXPECT_NE(worldHookSource.find("RenderPassState::ReleaseMainWorldModelPass(frame)"),
              std::string::npos);
    EXPECT_NE(viewModelHookSource.find("RenderPassState::IsMainWorldModelPassComplete(frame)"),
              std::string::npos);
    EXPECT_NE(viewModelHookSource.find("RenderPassState::TryBeginCompositePass(frame)"),
              std::string::npos);
    EXPECT_NE(viewModelHookSource.find("F::SpyCamera->IsRendering()"), std::string::npos);
}

TEST(MaterialsContracts, CompositeBelongsToRenderViewThatDrewMainWorld) {
    const auto root = testhelpers::FindRepoRoot();
    const auto passSource = testhelpers::ReadTextFile(root / kRenderPassStateSource);
    const auto renderViewSource = testhelpers::ReadTextFile(root / kRenderViewHookSource);
    const auto worldHookSource = testhelpers::ReadTextFile(root / kWorldRenderHookSource);
    const auto viewModelHookSource = testhelpers::ReadTextFile(root / kViewModelHookSource);

    EXPECT_NE(passSource.find("class CRenderViewScope"), std::string::npos);
    EXPECT_NE(renderViewSource.find("CRenderViewScope renderViewScope"), std::string::npos);
    EXPECT_NE(worldHookSource.find("MarkCurrentRenderViewMainWorld()"), std::string::npos);
    EXPECT_NE(viewModelHookSource.find("IsCurrentRenderViewMainWorld()"), std::string::npos);
}

TEST(MaterialsContracts, ViewmodelOnlyMaterialsInitializeOnDemand) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kHeaderSource);
    const auto modelHook = testhelpers::ReadTextFile(root / kModelRenderHookSource);

    EXPECT_NE(header.find("void EnsureInitialized();"), std::string::npos);
    EXPECT_NE(modelHook.find("F::Materials->EnsureInitialized();"), std::string::npos);
}

TEST(MaterialsContracts, FeatureRunHasLocalFrameGuardAndCleanupReset) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(mainSource.find("m_nLastRunFrame == frame"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nLastRunFrame = frame"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nLastRunFrame = -1"), std::string::npos);
    EXPECT_NE(mainSource.find("RenderPassState::ResetFrameGates()"), std::string::npos);

    const auto activeGuardPos = mainSource.find("if (!CFG::Materials_Active");
    const auto initializePos = mainSource.find("EnsureInitialized();", activeGuardPos);
    ASSERT_NE(activeGuardPos, std::string::npos);
    ASSERT_NE(initializePos, std::string::npos);
    EXPECT_LT(activeGuardPos, initializePos)
        << "Disabled/UI/screenshot paths must not initialize material resources.";
}
