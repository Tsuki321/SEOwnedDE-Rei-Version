#include <gtest/gtest.h>

#include "App/Features/MovementSimulation/MovementSimulation.h"
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
    EXPECT_NE(mainSource.find("GetRefEHandle().ToInt()"), std::string::npos);
    EXPECT_NE(mainSource.find("m_flDeathTime()"), std::string::npos);
    EXPECT_NE(mainSource.find("MAX_HISTORY_GAP_TICKS"), std::string::npos);
    EXPECT_NE(mainSource.find("IsStrafePredictionActive()"), std::string::npos);
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

TEST(MovementSimulationContracts, AirStrafeYawRateDecaysAcrossHorizon) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;

    ASSERT_TRUE(std::filesystem::exists(mainPath));

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    // ground strafe keeps its settle ramp
    EXPECT_NE(mainSource.find("Math::RemapValClamped(flTimeToTarget, 0.0f, 1.0f, 1.0f, 0.5f)"), std::string::npos);
    // air strafe decays the per-snapshot turn rate toward zero so a single
    // snapshot cannot curve the whole prediction horizon
    EXPECT_NE(mainSource.find("powf(flAirDecayPerTick, static_cast<float>(TIME_TO_TICKS(std::max(flTimeToTarget, 0.0f))))"), std::string::npos);
}

TEST(MovementPredictionMath, YawDeltaPreservesDirectionAcrossWraparound) {
    EXPECT_FLOAT_EQ(MovementPredictionMath::NormalizeYawDelta(5.0f, 355.0f), 10.0f);
    EXPECT_FLOAT_EQ(MovementPredictionMath::NormalizeYawDelta(355.0f, 5.0f), -10.0f);
}

TEST(MovementPredictionMath, YawDeltaUsesActualSampleTime) {
    constexpr float tickInterval = 1.0f / 66.0f;

    EXPECT_NEAR(MovementPredictionMath::ComputeYawDeltaPerTick(
        30.0f, 0.0f, tickInterval * 3.0f, tickInterval), 10.0f, 1e-5f);
    EXPECT_NEAR(MovementPredictionMath::ComputeYawDeltaPerTick(
        330.0f, 0.0f, tickInterval * 3.0f, tickInterval), -10.0f, 1e-5f);
    EXPECT_FLOAT_EQ(MovementPredictionMath::ComputeYawDeltaPerTick(
        30.0f, 0.0f, 0.0f, tickInterval), 0.0f);
}

TEST(MovementPredictionMath, DotProjectionHandlesAxisAlignedBasis) {
    Vec3 forward = {};
    Vec3 right = {};
    Math::AngleVectors({ 0.0f, 0.0f, 0.0f }, &forward, &right, nullptr);

    float forwardMove = 0.0f;
    float sideMove = 0.0f;
    MovementPredictionMath::ProjectHorizontalVelocity(
        { 120.0f, -30.0f, 15.0f }, forward, right, forwardMove, sideMove);

    EXPECT_NEAR(forwardMove, 120.0f, 1e-5f);
    EXPECT_NEAR(sideMove, 30.0f, 1e-5f);
    EXPECT_NEAR(forward.x * forwardMove + right.x * sideMove, 120.0f, 1e-5f);
    EXPECT_NEAR(forward.y * forwardMove + right.y * sideMove, -30.0f, 1e-5f);
}

TEST(MovementPredictionMath, SignedRotationPreservesHorizontalSpeed) {
    const Vec3 left = MovementPredictionMath::RotateHorizontalVelocity({ 100.0f, 0.0f, 7.0f }, 10.0f);
    const Vec3 right = MovementPredictionMath::RotateHorizontalVelocity({ 100.0f, 0.0f, 7.0f }, -10.0f);

    EXPECT_GT(left.y, 0.0f);
    EXPECT_LT(right.y, 0.0f);
    EXPECT_NEAR(left.Length2D(), 100.0f, 1e-4f);
    EXPECT_NEAR(right.Length2D(), 100.0f, 1e-4f);
    EXPECT_FLOAT_EQ(left.z, 7.0f);
}

TEST(MovementPredictionMath, ReconcileTurnComposesAccelerationAndCurvature) {
    const Vec3 previous = { 100.0f, 0.0f, 0.0f };
    const Vec3 accelerated = MovementPredictionMath::RotateHorizontalVelocity(previous, 3.0f);
    const Vec3 reconciled = MovementPredictionMath::ReconcileHorizontalTurn(previous, accelerated, -6.0f);

    EXPECT_NEAR(MovementPredictionMath::NormalizeYawDelta(
        Math::VelocityToAngles(reconciled).y, Math::VelocityToAngles(previous).y), -6.0f, 1e-4f);
    EXPECT_NEAR(reconciled.Length2D(), accelerated.Length2D(), 1e-4f);
}
