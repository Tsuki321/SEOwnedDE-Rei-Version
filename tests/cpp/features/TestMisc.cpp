#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Misc";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Misc/Misc.cpp";
}

TEST(MiscContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(MiscContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Misc_Bunnyhop"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Entities"), std::string::npos);
    EXPECT_NE(mainSource.find("CMisc::Bunnyhop("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(MiscContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(9));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(4));
}

TEST(MiscContracts, AutoMedigunStampsLivePoseThroughGetCommandTick) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMainSource);
    const auto autoMedigun = src.find("void CMisc::AutoMedigun");
    ASSERT_NE(autoMedigun, std::string::npos);

    const auto body = src.substr(autoMedigun);
    const auto tickWrite = body.find("cmd->tick_count =");
    ASSERT_NE(tickWrite, std::string::npos);

    const auto accuracyGate = body.rfind("CFG::Misc_Accuracy_Improvements", tickWrite);
    ASSERT_NE(accuracyGate, std::string::npos);
    EXPECT_LT(accuracyGate, tickWrite);
    EXPECT_NE(body.find("CLagRecords::GetCommandTick(pl->m_flSimulationTime())"), std::string::npos);
    EXPECT_EQ(body.find("TIME_TO_TICKS(pl->m_flSimulationTime()"), std::string::npos);

    const auto claim = body.find("G::bCommandTickResolved = true;", tickWrite);
    ASSERT_NE(claim, std::string::npos);
    EXPECT_LT(tickWrite, claim);
}
