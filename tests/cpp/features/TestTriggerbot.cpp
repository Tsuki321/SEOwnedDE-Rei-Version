#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/Triggerbot.cpp";
constexpr const char* kAutoBackstabSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/AutoBackstab/AutoBackstab.cpp";
constexpr const char* kAutoShootSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/AutoShoot/AutoShoot.cpp";
constexpr const char* kAutoVaccinatorSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/AutoVaccinator/AutoVaccinator.cpp";
}

TEST(TriggerbotContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(4));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(4));
}

TEST(TriggerbotContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Triggerbot_Active"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Input"), std::string::npos);
    EXPECT_NE(mainSource.find("CTriggerbot::Run("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(TriggerbotContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(13));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(4));
}

TEST(TriggerbotContracts, AutoBackstabUsesMoveChildRazorbackWalk) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kAutoBackstabSource);

    EXPECT_NE(src.find("HasActiveRazorback"), std::string::npos);
    EXPECT_NE(src.find("FirstMoveChild()"), std::string::npos);
    EXPECT_NE(src.find("NextMovePeer()"), std::string::npos);
    EXPECT_NE(src.find("CTFWearableRazorback"), std::string::npos);
    // Must not scan the full client entity list for razorbacks.
    EXPECT_EQ(src.find("GetHighestEntityIndex()"), std::string::npos);
}

TEST(TriggerbotContracts, AutoShootLeavesManualHitscanTickOwnedByAimbot) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kAutoShootSource);
    const auto manualGuard = src.find("if (G::bManualHitscanFiring)");
    const auto trace = src.find("H::AimUtils->Trace(", manualGuard);
    const auto tickWrite = src.find("pCmd->tick_count =", manualGuard);

    ASSERT_NE(manualGuard, std::string::npos);
    ASSERT_NE(trace, std::string::npos);
    EXPECT_LT(manualGuard, trace);
    // AutoShoot must leave the incoming tick or the aimbot's historical tick
    // untouched; it does not fabricate a tick for the live trace.
    EXPECT_EQ(tickWrite, std::string::npos);
    EXPECT_NE(src.find("G::bCommandTickResolved = true;", trace), std::string::npos);
}

TEST(TriggerbotContracts, AutoShootCannotBypassAimbotFireDelay) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kAutoShootSource);
    const auto delayGuard = src.find("if (G::bAimbotFireDelayed)");
    const auto trace = src.find("H::AimUtils->Trace(", delayGuard);

    ASSERT_NE(delayGuard, std::string::npos);
    ASSERT_NE(trace, std::string::npos);
    EXPECT_LT(delayGuard, trace);
}

TEST(TriggerbotContracts, AutoBackstabUsesAuthoritativeHullAndRecordWindow) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kAutoBackstabSource);

    EXPECT_EQ(src.find("kMaxBackstabCandidateRange"), std::string::npos);
    EXPECT_EQ(src.find("Triggerbot_AutoBackstab_Max_Backtrack_Time"), std::string::npos);
    EXPECT_EQ(src.find("CLagRecords::GetRecordAge"), std::string::npos);
    EXPECT_NE(src.find("CLagRecords::IsRecordUsable"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(src, "H::AimUtils->TraceEntityMelee"), 2u);
    EXPECT_EQ(testhelpers::CountOccurrences(src, "H::AimUtils->IsBehindAndFacingTarget"), 2u);
    EXPECT_EQ(src.find("bool IsBehindAndFacingTarget("), std::string::npos);
}

TEST(TriggerbotContracts, AutoBackstabDoesNotRetargetManualMelee) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kAutoBackstabSource);

    const auto manualGuard = src.find("if (G::bManualMeleeFiring)");
    const auto targetLoop = src.find("GetGroup(EEntGroup::PLAYERS_ENEMIES)");
    ASSERT_NE(manualGuard, std::string::npos);
    ASSERT_NE(targetLoop, std::string::npos);
    EXPECT_LT(manualGuard, targetLoop);
}

TEST(TriggerbotContracts, ExpensiveQueriesFollowCheapClassification) {
    const auto root = testhelpers::FindRepoRoot();
    const auto autoShoot = testhelpers::ReadTextFile(root / kAutoShootSource);
    const auto autoVaccinator = testhelpers::ReadTextFile(root / kAutoVaccinatorSource);
    const auto autoBackstab = testhelpers::ReadTextFile(root / kAutoBackstabSource);

    EXPECT_EQ(testhelpers::CountOccurrences(autoShoot, "H::AimUtils->Trace("), 1u);
    EXPECT_EQ(autoShoot.find("PLAYERS_ENEMIES"), std::string::npos);

    const auto projectileSection = autoVaccinator.find("// Check for dangerous projectiles");
    const auto distanceCheck = autoVaccinator.find("const float flDistanceSqr", projectileSection);
    const auto visibilityTrace = autoVaccinator.find("const bool bVisible", distanceCheck);
    ASSERT_NE(projectileSection, std::string::npos);
    ASSERT_NE(distanceCheck, std::string::npos);
    ASSERT_NE(visibilityTrace, std::string::npos);
    EXPECT_LT(distanceCheck, visibilityTrace);
    EXPECT_NE(autoVaccinator.find("|| H::AimUtils->TraceEntityAutoDet", visibilityTrace), std::string::npos);

    const auto targetCenter = autoBackstab.find("const Vec3 vTargetCenter");
    const auto razorbackWalk = autoBackstab.find("HasActiveRazorback(pPlayer)", targetCenter);
    const auto meleeTrace = autoBackstab.find("H::AimUtils->TraceEntityMelee", razorbackWalk);
    ASSERT_NE(targetCenter, std::string::npos);
    ASSERT_NE(razorbackWalk, std::string::npos);
    ASSERT_NE(meleeTrace, std::string::npos);
    EXPECT_LT(targetCenter, razorbackWalk);
    EXPECT_LT(razorbackWalk, meleeTrace);
}
