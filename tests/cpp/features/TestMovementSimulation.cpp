#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/MovementSimulation";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/MovementSimulation/MovementSimulation.cpp";
}

TEST(MovementSimulationContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(MovementSimulationContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Aimbot_Projectile_Aim_Prediction_Method"), std::string::npos);
    EXPECT_NE(mainSource.find("Aimbot_Projectile_Aim_Prediction_Method == 1"), std::string::npos);
    EXPECT_NE(mainSource.find("m_vAdaptiveVelocity"), std::string::npos);
    EXPECT_NE(mainSource.find("m_vAccelTrend"), std::string::npos);
    EXPECT_NE(mainSource.find("flRecordConfidence"), std::string::npos);
    EXPECT_NE(mainSource.find("flSumTimeVelX"), std::string::npos);
    EXPECT_NE(mainSource.find("m_vAdaptiveVelocity.Length2D() > 1.0f"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Entities"), std::string::npos);
    EXPECT_NE(mainSource.find("CPlayerDataBackup::Store("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(MovementSimulationContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(8));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(1));
}
