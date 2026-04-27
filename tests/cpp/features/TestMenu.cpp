#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu/Menu.cpp";
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
    EXPECT_NE(mainSource.find("SelectSingle(\"Prediction Method\", CFG::Aimbot_Projectile_Aim_Prediction_Method"), std::string::npos);
    EXPECT_NE(mainSource.find("{ \"Full Acceleration\", 0 }"), std::string::npos);
    EXPECT_NE(mainSource.find("{ \"Adaptive Tracking\", 3 }"), std::string::npos);
    EXPECT_EQ(mainSource.find("Aimbot_Projectile_Aim_Prediction_Method == 4"), std::string::npos);
    EXPECT_EQ(mainSource.find("Aimbot_Projectile_Aim_Prediction_Method == 5"), std::string::npos);
}
