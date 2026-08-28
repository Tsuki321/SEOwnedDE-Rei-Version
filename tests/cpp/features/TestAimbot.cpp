#include <gtest/gtest.h>

#include "App/Features/Aimbot/AimbotProjectile/AimbotProjectilePrediction.h"

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/Aimbot.cpp";
constexpr const char* kProjectileSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotProjectile/AimbotProjectile.cpp";
constexpr const char* kProjectileHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotProjectile/AimbotProjectile.h";
constexpr const char* kCreateMoveSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/ClientModeShared_CreateMove.cpp";
constexpr const char* kLevelInitSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_LevelInitPostEntity.cpp";
constexpr const char* kLevelShutdownSource = "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_LevelShutdown.cpp";
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
    EXPECT_NE(projectileSource.find("F::AimbotCommon->Sort(m_vecTargets, nSortMode)"), std::string::npos);
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
    const auto hitscanScan = hitscanSource.find("GetTarget(pLocal, pWeapon, target)");
    const auto meleeGuard = meleeSource.find("if (!needsTargetScan)");
    const auto meleeScan = meleeSource.find("GetTarget(pLocal, pWeapon, target)");

    ASSERT_NE(hitscanGuard, std::string::npos);
    ASSERT_NE(hitscanScan, std::string::npos);
    ASSERT_NE(meleeGuard, std::string::npos);
    ASSERT_NE(meleeScan, std::string::npos);
    EXPECT_LT(hitscanGuard, hitscanScan);
    EXPECT_LT(meleeGuard, meleeScan);

    EXPECT_NE(hitscanSource.find("SetupHitboxScan"), std::string::npos);
    EXPECT_NE(hitscanSource.find("BONE_USED_BY_HITBOX"), std::string::npos);
    EXPECT_EQ(hitscanSource.find("GetHitboxPos(n)"), std::string::npos);

	EXPECT_NE(projectileSource.find("m_TargetStates.push_back"), std::string::npos);
	EXPECT_NE(projectileSource.find("refineResult.Tick"), std::string::npos);
	EXPECT_NE(projectilePredictionHeader.find("FindPositiveQuarticRoots"), std::string::npos);
	EXPECT_NE(projectilePredictionHeader.find("HorizonLimited"), std::string::npos);

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

TEST(AimbotContracts, ProjectileChargeHoldIsWeaponBoundAndLossSafe) {
	const auto root = testhelpers::FindRepoRoot();
	const auto projectileSource = testhelpers::ReadTextFile(root / kProjectileSource);
	const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);
	const auto createMoveSource = testhelpers::ReadTextFile(root / kCreateMoveSource);

	EXPECT_NE(projectileSource.find("m_ChargeHold.Weapon = pWeapon"), std::string::npos);
	EXPECT_NE(projectileSource.find("m_ChargeHold.Target = target.Entity"), std::string::npos);
	EXPECT_NE(projectileSource.find("m_ChargeHold.LastSolvedAngle = target.AngleTo"), std::string::npos);
	EXPECT_NE(projectileSource.find("m_ChargeHold.LastSolvedCommandNumber = pCmd->command_number"), std::string::npos);
	EXPECT_NE(projectileSource.find("m_ChargeHold.Weapon.Get() == pWeapon"), std::string::npos);
	EXPECT_NE(projectileSource.find("MaintainChargeHold(pCmd, pLocal, pWeapon)"), std::string::npos);
	EXPECT_NE(projectileSource.find("QueueChargeRelease(pCmd)"), std::string::npos);
	EXPECT_NE(projectileSource.find("QueueChargeRelease(pCmd, true)"), std::string::npos);
	EXPECT_NE(projectileSource.find("m_ChargeHold.ChargeObserved"), std::string::npos);
	EXPECT_NE(projectileSource.find("m_ChargeHold.AbortRelease"), std::string::npos);
	EXPECT_NE(projectileSource.find("pCmd->buttons |= IN_ATTACK;"), std::string::npos);
	EXPECT_NE(projectileSource.find("ResetChargeHold();"), std::string::npos);

	const auto maintainCharge = projectileSource.find("bool CAimbotProjectile::MaintainChargeHold");
	const auto staleCharge = projectileSource.find("m_ChargeHold.ChargeObserved || !pWeapon->HasPrimaryAmmoForShot()", maintainCharge);
	const auto stoppedContext = projectileSource.find("const bool bContextStopped", maintainCharge);
	const auto abortRelease = projectileSource.find("QueueChargeRelease(pCmd, true)", stoppedContext);
	const auto lossSafeHold = projectileSource.find("pCmd->buttons |= IN_ATTACK;", abortRelease);
	ASSERT_NE(maintainCharge, std::string::npos);
	ASSERT_NE(staleCharge, std::string::npos);
	ASSERT_NE(stoppedContext, std::string::npos);
	ASSERT_NE(abortRelease, std::string::npos);
	ASSERT_NE(lossSafeHold, std::string::npos);
	EXPECT_LT(staleCharge, stoppedContext);
	EXPECT_LT(stoppedContext, abortRelease);
	EXPECT_LT(abortRelease, lossSafeHold);

	const auto mainLifecycle = mainSource.find("RunChargeLifecycle(pCmd, pLocal, pWeapon)");
	const auto mainDispatch = mainSource.find("RunMain(pCmd)");
	ASSERT_NE(mainLifecycle, std::string::npos);
	ASSERT_NE(mainDispatch, std::string::npos);
	EXPECT_LT(mainLifecycle, mainDispatch);

	const auto createLifecycle = createMoveSource.find("RunChargeLifecycle(pCmd, pLocal, pWeapon)");
	const auto sendPacket = createMoveSource.find("bool* pSendPacket");
	const auto rapidFireExit = createMoveSource.find("ShouldExitCreateMove(pCmd)");
	const auto preSilentFinalize = createMoveSource.find("FinalizeChargeCommand(pCmd, pLocal, pWeapon, false)");
	const auto pseudoSilent = createMoveSource.find("//pSilent");
	const auto finalizeCharge = createMoveSource.rfind("FinalizeChargeCommand(pCmd, pLocal, pWeapon)");
	const auto finalRapidFire = createMoveSource.find("F::RapidFire->Run(pCmd, pSendPacket)");
	const auto recordButtons = createMoveSource.find("G::nOldButtons = pCmd->buttons");
	ASSERT_NE(createLifecycle, std::string::npos);
	ASSERT_NE(sendPacket, std::string::npos);
	ASSERT_NE(rapidFireExit, std::string::npos);
	ASSERT_NE(preSilentFinalize, std::string::npos);
	ASSERT_NE(pseudoSilent, std::string::npos);
	ASSERT_NE(finalizeCharge, std::string::npos);
	ASSERT_NE(finalRapidFire, std::string::npos);
	ASSERT_NE(recordButtons, std::string::npos);
	EXPECT_LT(sendPacket, createLifecycle);
	EXPECT_LT(createLifecycle, rapidFireExit);
	EXPECT_LT(preSilentFinalize, pseudoSilent);
	EXPECT_LT(finalRapidFire, finalizeCharge);
	EXPECT_LT(finalizeCharge, recordButtons);
	EXPECT_GE(testhelpers::CountOccurrences(createMoveSource, "FinalizeChargeCommand(pCmd, pLocal, pWeapon)"), 2u);
	EXPECT_GE(testhelpers::CountOccurrences(createMoveSource, "if (G::bPSilentAngles)"), 3u);
}

TEST(AimbotContracts, ProjectileSplashAndMultipointUseFinalTimingAndLaunchState) {
	const auto root = testhelpers::FindRepoRoot();
	const auto projectileSource = testhelpers::ReadTextFile(root / kProjectileSource);

	EXPECT_NE(projectileSource.find("flTimingBias + flRemainingChargeWait + flImpactTime"), std::string::npos);
	EXPECT_NE(projectileSource.find("GetTargetStateAtTime(flTargetTime)"), std::string::npos);
	EXPECT_NE(projectileSource.find("GetProjectileHull(launch, projectileMins, projectileMaxs)"), std::string::npos);
	EXPECT_NE(projectileSource.find("vImpact.DistTo(vTargetPoint) > radius"), std::string::npos);
	EXPECT_NE(projectileSource.find("constexpr float kRocketExplosionRadius = 146.0f"), std::string::npos);
	EXPECT_NE(projectileSource.find("AttribHookValue(kRocketExplosionRadius, \"mult_explosion_radius\", pWeapon)"), std::string::npos);
	EXPECT_EQ(projectileSource.find("radius = 130.0f"), std::string::npos);
	EXPECT_NE(projectileSource.find("mpPath, TICK_INTERVAL, mpRefine.Tick, mpLaunch.m_pos"), std::string::npos);
	EXPECT_NE(projectileSource.find("mpRefine.SimulatedTime - (target.TimeToTarget + flTimingBias)"), std::string::npos);
	EXPECT_EQ(projectileSource.find("traceVal.fraction < 0.9f"), std::string::npos);
}

TEST(AimbotContracts, ProjectileLeadMatchesPoseClockAndFireAngles) {
	const auto root = testhelpers::FindRepoRoot();
	const auto projectileSource = testhelpers::ReadTextFile(root / kProjectileSource);

	// accuracy mode extrapolates from the newest network pose, so the lead must
	// not cross the interpolation window on top of the outgoing latency
	EXPECT_NE(projectileSource.find("CFG::Misc_Accuracy_Improvements ? 0.0f : SDKUtils::GetLerp()"), std::string::npos);
	// the pseudo-silent packet delay only applies to a command that is actually choked
	EXPECT_NE(projectileSource.find("bUsesPseudoSilent && bCommandChoked ? TICK_INTERVAL : 0.0f"), std::string::npos);
	// projectiles fire along the command viewangles without punch, unlike hitscan
	EXPECT_EQ(projectileSource.find("vAngles - pLocal->m_vecPunchAngle()"), std::string::npos);
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
    EXPECT_NE(projectileSource.find("F::AimbotCommon->Sort(m_vecTargets, nSortMode)"), std::string::npos);
}

TEST(AimbotContracts, ProjectileDelayStateResetsAcrossLevelTransitions) {
    const auto root = testhelpers::FindRepoRoot();
    const auto projectileHeader = testhelpers::ReadTextFile(root / kProjectileHeader);
    const auto levelInit = testhelpers::ReadTextFile(root / kLevelInitSource);
    const auto levelShutdown = testhelpers::ReadTextFile(root / kLevelShutdownSource);

    EXPECT_NE(projectileHeader.find("m_flDelayFireEndTime = 0.0f"), std::string::npos);
    EXPECT_NE(projectileHeader.find("m_nLastFiredTargetIndex = -1"), std::string::npos);
    EXPECT_NE(projectileHeader.find("m_flTargetSwitchEndTime = 0.0f"), std::string::npos);
    EXPECT_NE(levelInit.find("F::AimbotProjectile->Reset()"), std::string::npos);
    EXPECT_NE(levelShutdown.find("F::AimbotProjectile->Reset()"), std::string::npos);
}

TEST(AimbotContracts, HitscanAndProjectileFovConstrainEverySortMode) {
    const auto root = testhelpers::FindRepoRoot();
    const auto hitscanSource = testhelpers::ReadTextFile(root / kHitscanSource);
    const auto projectileSource = testhelpers::ReadTextFile(root / kProjectileSource);

    EXPECT_NE(hitscanSource.find("G::flAimbotFOV = CFG::Aimbot_Hitscan_FOV"), std::string::npos);
    EXPECT_NE(projectileSource.find("G::flAimbotFOV = CFG::Aimbot_Projectile_FOV"), std::string::npos);
    EXPECT_NE(projectileSource.find("const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo)"), std::string::npos);
    EXPECT_EQ(projectileSource.find("nSortMode == 0 ? Math::CalcFov"), std::string::npos);
    EXPECT_NE(projectileSource.find("if (flFOVTo > flFOVLimit)"), std::string::npos);
}
