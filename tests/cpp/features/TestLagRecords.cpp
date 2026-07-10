#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp";
}

TEST(LagRecordsContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(LagRecordsContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Misc_SetupBones_Optimization"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Entities"), std::string::npos);
    EXPECT_NE(mainSource.find("CLagRecords::IsSimulationTimeValid("), std::string::npos);
    // In-place ring build (commit 0df6959+): AddRecord aliases the destination
    // slot instead of move-assigning a stack-local record into it.
    EXPECT_NE(mainSource.find("LagRecord_t& newRecord = records[newHead]"), std::string::npos);
    EXPECT_NE(mainSource.find("m_LagRecords[i]"), std::string::npos);
    EXPECT_NE(mainSource.find("sizeof(matrix3x4_t) * entry.BoneCount"), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(LagRecordsContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(4));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(3));
}
