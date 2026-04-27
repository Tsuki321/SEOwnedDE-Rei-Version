#include <gtest/gtest.h>

#include "App/Features/Aimbot/AimbotProjectile/AimbotProjectilePrediction.h"

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/Aimbot.cpp";
constexpr const char* kProjectileSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotProjectile/AimbotProjectile.cpp";
constexpr const char* kCommonHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotCommon/AimbotCommon.h";
constexpr const char* kHitscanSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp";
constexpr const char* kMeleeSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotMelee/AimbotMelee.cpp";
}

TEST(AimbotContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(4));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(5));
}

TEST(AimbotContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Aimbot_Active"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Entities"), std::string::npos);
    EXPECT_NE(mainSource.find("CAimbot::RunMain("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(AimbotContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(51));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(23));
}

TEST(AimbotContracts, ProjectileTargetBudgetRemainsStrict) {
    const auto root = testhelpers::FindRepoRoot();
    const auto projectilePath = root / kProjectileSource;

    ASSERT_TRUE(std::filesystem::exists(projectilePath));

    const auto projectileSource = testhelpers::ReadTextFile(projectilePath);
    EXPECT_NE(projectileSource.find("F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Projectile_Sort)"), std::string::npos);
    EXPECT_NE(projectileSource.find("Aimbot_Projectile_Max_Processing_Targets"), std::string::npos);
    EXPECT_NE(projectileSource.find("auto targetsScanned{ 0 }"), std::string::npos);
    EXPECT_NE(projectileSource.find("targetsScanned >= maxTargets"), std::string::npos);
    EXPECT_NE(projectileSource.find("target.Position.DistTo(vLocalPos) > 400.0f"), std::string::npos);
    EXPECT_NE(projectileSource.find("targetsScanned++"), std::string::npos);
    EXPECT_NE(projectileSource.find("SolveTarget(pLocal, pWeapon, pCmd, target)"), std::string::npos);
}

TEST(AimbotPredictionMath, HybridBlendFactorDecaysProperly) {
    const float startConfidence = 1.0f;
    const float decayRate = 2.5f;

    const float tick0 = ProjectilePredictionMath::ComputeHybridBlendFactor(0, decayRate, startConfidence);
    const float tick10 = ProjectilePredictionMath::ComputeHybridBlendFactor(10, decayRate, startConfidence);
    const float tick50 = ProjectilePredictionMath::ComputeHybridBlendFactor(50, decayRate, startConfidence);

    EXPECT_FLOAT_EQ(tick0, 0.85f);
    EXPECT_LT(tick10, tick0);
    EXPECT_LT(tick50, tick10);
    EXPECT_GT(tick50, 0.149f); // should approach 0.15
}

TEST(AimbotPredictionMath, VelocityConfidenceScalesByVariance) {
    const float maxSpeedSq = 90000.0f; // 300^2
    const float zeroVariance = 0.0f;
    const float highVariance = 45000.0f; // 0.5 * maxSpeedSq
    const float extremeVariance = 180000.0f; // 2 * maxSpeedSq

    const float maxConfidence = ProjectilePredictionMath::ComputeVelocityConfidence(zeroVariance, maxSpeedSq);
    const float midConfidence = ProjectilePredictionMath::ComputeVelocityConfidence(highVariance, maxSpeedSq);
    const float minConfidence = ProjectilePredictionMath::ComputeVelocityConfidence(extremeVariance, maxSpeedSq);

    EXPECT_FLOAT_EQ(maxConfidence, 1.0f);
    EXPECT_FLOAT_EQ(midConfidence, 0.5f);
    EXPECT_FLOAT_EQ(minConfidence, 0.2f); // max clamped at 1.0 - 0.8
}

TEST(AimbotPredictionMath, StickyArmArrivalUsesArmTimeAsMinimum) {
    EXPECT_FLOAT_EQ(ProjectilePredictionMath::ApplyStickyArmTime(0.25f, 0.8f), 0.8f);
    EXPECT_FLOAT_EQ(ProjectilePredictionMath::ApplyStickyArmTime(1.2f, 0.8f), 1.2f);
}

TEST(AimbotPredictionMath, TemporalResidualIncludesOutgoingAndInterpBias) {
    const float interpolation = ProjectilePredictionMath::ResolveInterpolationAmount(0.03f, 0.015f);
    const float timingBias = ProjectilePredictionMath::ComputeTimingBias(0.05f, interpolation);
    const float residual = ProjectilePredictionMath::ComputeTemporalResidual(1.08f, 1.0f, timingBias);

    EXPECT_NEAR(residual, 0.0f, 1e-6f);
}

TEST(AimbotPredictionMath, TemporalToleranceAndScoreAreConsistent) {
    const float tolerance = 0.06f;
    const float nearResidual = 0.02f;
    const float farResidual = 0.08f;

    EXPECT_TRUE(ProjectilePredictionMath::IsWithinTemporalTolerance(nearResidual, tolerance));
    EXPECT_FALSE(ProjectilePredictionMath::IsWithinTemporalTolerance(farResidual, tolerance));
    EXPECT_LT(ProjectilePredictionMath::ComputeTemporalScore(nearResidual, tolerance),
              ProjectilePredictionMath::ComputeTemporalScore(farResidual, tolerance));

    const float nearCombined = ProjectilePredictionMath::CombineTemporalAndSpatialScore(0.5f, 2.0f, 0.35f);
    const float farCombined = ProjectilePredictionMath::CombineTemporalAndSpatialScore(0.5f, 30.0f, 0.35f);
    EXPECT_LT(nearCombined, farCombined);
}

TEST(AimbotContracts, SharedTargetScoringIsCentralizedAcrossModes) {
    const auto root = testhelpers::FindRepoRoot();

    const auto commonPath = root / kCommonHeader;
    const auto hitscanPath = root / kHitscanSource;
    const auto meleePath = root / kMeleeSource;
    const auto projectilePath = root / kProjectileSource;

    ASSERT_TRUE(std::filesystem::exists(commonPath));
    ASSERT_TRUE(std::filesystem::exists(hitscanPath));
    ASSERT_TRUE(std::filesystem::exists(meleePath));
    ASSERT_TRUE(std::filesystem::exists(projectilePath));

    const auto commonSource = testhelpers::ReadTextFile(commonPath);
    const auto hitscanSource = testhelpers::ReadTextFile(hitscanPath);
    const auto meleeSource = testhelpers::ReadTextFile(meleePath);
    const auto projectileSource = testhelpers::ReadTextFile(projectilePath);

    EXPECT_NE(commonSource.find("void Sort(std::vector<T>& targets, int sortMode)"), std::string::npos);
    EXPECT_NE(commonSource.find("std::ranges::sort(targets"), std::string::npos);
    EXPECT_NE(hitscanSource.find("F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Hitscan_Sort)"), std::string::npos);
    EXPECT_NE(meleeSource.find("F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Melee_Sort)"), std::string::npos);
    EXPECT_NE(projectileSource.find("F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Projectile_Sort)"), std::string::npos);
}
