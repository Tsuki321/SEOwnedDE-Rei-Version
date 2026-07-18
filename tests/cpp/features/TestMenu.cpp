#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu/Menu.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu/Menu.h";
}

TEST(MenuContracts, AvoidsPerFrameModalVectorAndFilesystemChurn) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);
    const auto headerSource = testhelpers::ReadTextFile(root / kHeaderSource);

    EXPECT_NE(headerSource.find("m_pActiveControl"), std::string::npos);
    EXPECT_EQ(headerSource.find("m_mapStates"), std::string::npos);
    EXPECT_NE(headerSource.find("std::initializer_list<std::pair<const char *, int>>"), std::string::npos);
    EXPECT_NE(mainSource.find("flNextConfigRefresh"), std::string::npos);
    EXPECT_NE(mainSource.find("Plat_FloatTime()"), std::string::npos);
    EXPECT_NE(mainSource.find("MAX_SNOW_FLAKES = 400"), std::string::npos);

    const auto openGuard = mainSource.find("if (m_bOpen)");
    const auto gradientAllocation = mainSource.find("m_pGradient = std::make_unique", openGuard);
    ASSERT_NE(openGuard, std::string::npos);
    ASSERT_NE(gradientAllocation, std::string::npos);
    EXPECT_GT(gradientAllocation, openGuard);
}

TEST(MenuContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(2));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(2));
}

TEST(MenuContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Menu_Spacing_Y"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Input"), std::string::npos);
    EXPECT_NE(mainSource.find("CMenu::Drag("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(MenuContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(28));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(9));
}

TEST(MenuContracts, ProjectilePredictionMethodShowsRelevantTuningControlsOnly) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;

    ASSERT_TRUE(std::filesystem::exists(mainPath));

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CheckBox(\"High Arc\", CFG::Aimbot_Projectile_High_Arc)"), std::string::npos);
    EXPECT_NE(mainSource.find("SelectSingle(\"Prediction Method\", CFG::Aimbot_Projectile_Aim_Prediction_Method"), std::string::npos);
    EXPECT_NE(mainSource.find("{ \"Constant Velocity\", 0 }"), std::string::npos);
    EXPECT_NE(mainSource.find("{ \"Acceleration Tracking\", 1 }"), std::string::npos);
    EXPECT_EQ(mainSource.find("Aimbot_Projectile_Aim_Prediction_Method == 4"), std::string::npos);
    EXPECT_EQ(mainSource.find("Aimbot_Projectile_Aim_Prediction_Method == 5"), std::string::npos);
}
