#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Crits";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Crits/Crits.cpp";
constexpr const char* kMainHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/Crits/Crits.h";
constexpr const char* kConfigSource = "SEOwnedDE/SEOwnedDE/src/App/Features/CFG.h";
constexpr const char* kMenuSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu/Menu.cpp";
constexpr const char* kPaintHookSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/IEngineVGuiInternal_Paint.cpp";
constexpr const char* kEventHookSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/CGameEventManager_FireEventIntern.cpp";
}

TEST(CritsContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(CritsContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Exploits_Crits_Force_Crit_Key_Melee"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Entities"), std::string::npos);
    EXPECT_NE(mainSource.find("SDKUtils::RandomSeed("), std::string::npos);
    EXPECT_NE(mainSource.find("GetCritRequest("), std::string::npos);
    EXPECT_NE(mainSource.find("void CCrits::Paint()"), std::string::npos);
    EXPECT_NE(mainSource.find("void CCrits::Event("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(CritsContracts, IndicatorAndHookIntegrationTokensExist) {
    const auto root = testhelpers::FindRepoRoot();

    const auto cfgSource = testhelpers::ReadTextFile(root / kConfigSource);
    const auto menuSource = testhelpers::ReadTextFile(root / kMenuSource);
    const auto paintHookSource = testhelpers::ReadTextFile(root / kPaintHookSource);
    const auto eventHookSource = testhelpers::ReadTextFile(root / kEventHookSource);

    EXPECT_NE(cfgSource.find("CFGVAR(Exploits_Crits_Draw_Indicator"), std::string::npos);
    EXPECT_NE(menuSource.find("CFG::Exploits_Crits_Draw_Indicator"), std::string::npos);
    EXPECT_NE(paintHookSource.find("F::Crits->Paint();"), std::string::npos);
    EXPECT_NE(eventHookSource.find("F::Crits->Event(event, eventHash);"), std::string::npos);
}

TEST(CritsContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(3));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(1));
}

TEST(CritsContracts, ForecastIsCachedAndProbeWorkIsBounded) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kMainSource);
    const auto header = testhelpers::ReadTextFile(root / kMainHeader);

    EXPECT_NE(header.find("ForecastState_t"), std::string::npos);
    EXPECT_NE(header.find("m_iLastInfoTick"), std::string::npos);
    EXPECT_NE(source.find("m_ForecastState"), std::string::npos);
    EXPECT_NE(source.find("countAffordableCrits"), std::string::npos);
    EXPECT_NE(source.find("iAvailableCrits + 1"), std::string::npos);
    EXPECT_NE(source.find("iAvailableCrits >= BUCKET_ATTEMPTS"), std::string::npos);
    EXPECT_EQ(source.find("for (int j = 0; j < BUCKET_ATTEMPTS; j++)"), std::string::npos);

    const auto cheapRejects = source.find("pLocal->IsCritBoosted()");
    const auto updateInfo = source.find("UpdateInfo(pLocal, pWeapon);", cheapRejects);
    ASSERT_NE(cheapRejects, std::string::npos);
    ASSERT_NE(updateInfo, std::string::npos);
    EXPECT_LT(cheapRejects, updateInfo);
}

TEST(CritsContracts, ScreenshotStateUsesTheFrameCache) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(source.find("F::VisualUtils->IsTakingScreenshotCached()"), std::string::npos);
    EXPECT_EQ(source.find("I::EngineClient->IsTakingScreenshot()"), std::string::npos);
}
