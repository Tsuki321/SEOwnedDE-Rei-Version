#include <gtest/gtest.h>

#include "App/Features/Aimbot/AimbotProjectile/AimbotProjectilePrediction.h"

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/Aimbot.cpp";
constexpr const char* kProjectileSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotProjectile/AimbotProjectile.cpp";
constexpr const char* kProjectilePredictionHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotProjectile/AimbotProjectilePrediction.h";
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
    EXPECT_NE(projectileSource.find("targetsScanned++"), std::string::npos);
    EXPECT_NE(projectileSource.find("break;"), std::string::npos);
    EXPECT_NE(projectileSource.find("SolveTarget(pLocal, pWeapon, pCmd, target)"), std::string::npos);
}

TEST(AimbotContracts, HotPathWorkIsDemandDrivenAndBatched) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hitscanSource = testhelpers::ReadTextFile(root / kHitscanSource);
    const auto meleeSource = testhelpers::ReadTextFile(root / kMeleeSource);
    const auto projectileSource = testhelpers::ReadTextFile(root / kProjectileSource);
    const auto projectilePredictionHeader = testhelpers::ReadTextFile(root / kProjectilePredictionHeader);

    const auto hitscanGuard = hitscanSource.find("if (!needsTargetScan)");
    const auto hitscanScan = hitscanSource.find("if (GetTarget(pLocal, pWeapon, target)");
    const auto meleeGuard = meleeSource.find("if (!needsTargetScan)");
    const auto meleeScan = meleeSource.find("if (GetTarget(pLocal, pWeapon, target)");

    ASSERT_NE(hitscanGuard, std::string::npos);
    ASSERT_NE(hitscanScan, std::string::npos);
    ASSERT_NE(meleeGuard, std::string::npos);
    ASSERT_NE(meleeScan, std::string::npos);
    EXPECT_LT(hitscanGuard, hitscanScan);
    EXPECT_LT(meleeGuard, meleeScan);

    EXPECT_NE(hitscanSource.find("SetupHitboxScan"), std::string::npos);
    EXPECT_NE(hitscanSource.find("BONE_USED_BY_HITBOX"), std::string::npos);
    EXPECT_EQ(hitscanSource.find("GetHitboxPos(n)"), std::string::npos);

    EXPECT_NE(projectileSource.find("simulateToTickCount"), std::string::npos);
    EXPECT_NE(projectileSource.find("refineResult.Tick"), std::string::npos);
    EXPECT_NE(projectilePredictionHeader.find("if (tNext == tLo)"), std::string::npos);

    EXPECT_NE(projectileSource.find("GetRocketSplashSpherePoints"), std::string::npos);
    EXPECT_NE(projectileSource.find("std::array<RocketSplashCandidate"), std::string::npos);
    EXPECT_EQ(projectileSource.find("std::vector<Vec3> potential"), std::string::npos);
}

TEST(AimbotPredictionMath, TemporalResidualIncludesOutgoingAndInterpBias) {
	const float timingBias = ProjectilePredictionMath::ComputeTimingBias(0.05f, 0.03f);
	const float residual = ProjectilePredictionMath::ComputeTemporalResidual(1.08f, 1.0f, timingBias);

	EXPECT_NEAR(residual, 0.0f, 1e-6f);
}

TEST(AimbotPredictionMath, TemporalToleranceClampsNegativeInputs) {
	const float tolerance = 0.06f;
	const float nearResidual = 0.02f;
	const float farResidual = 0.08f;

	EXPECT_TRUE(ProjectilePredictionMath::IsWithinTemporalTolerance(nearResidual, tolerance));
	EXPECT_FALSE(ProjectilePredictionMath::IsWithinTemporalTolerance(farResidual, tolerance));
	EXPECT_TRUE(ProjectilePredictionMath::IsWithinTemporalTolerance(-nearResidual, tolerance));
	EXPECT_TRUE(ProjectilePredictionMath::IsWithinTemporalTolerance(0.0f, -tolerance));
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
    EXPECT_NE(commonSource.find("void SortFirst(std::vector<T>& targets"), std::string::npos);
    EXPECT_NE(commonSource.find("std::ranges::partial_sort(targets"), std::string::npos);
    EXPECT_NE(meleeSource.find("F::AimbotCommon->SortFirst(m_vecTargets"), std::string::npos);
    EXPECT_NE(projectileSource.find("F::AimbotCommon->Sort(m_vecTargets, CFG::Aimbot_Projectile_Sort)"), std::string::npos);
}
