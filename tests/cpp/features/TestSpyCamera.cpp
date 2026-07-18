#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/SpyCamera";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/SpyCamera/SpyCamera.cpp";
}

TEST(SpyCameraContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(SpyCameraContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Visuals_SpyCamera_Pos_X"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Input"), std::string::npos);
    EXPECT_NE(mainSource.find("CSpyCamera::Drag("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(SpyCameraContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(3));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(1));
}

TEST(SpyCameraContracts, ReusesValidatedTargetBetweenBoundedScans) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(mainSource.find("SPY_SCAN_INTERVAL_TICKS = 3"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nCachedSpyIndex"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nCachedSpyHandle"), std::string::npos);
    EXPECT_NE(mainSource.find("bTickRolledBack"), std::string::npos);
    EXPECT_NE(mainSource.find("pPlayer->m_iTeamNum() == pLocal->m_iTeamNum()"), std::string::npos);
    EXPECT_NE(mainSource.find("pPlayer->IsDormant()"), std::string::npos);
    EXPECT_NE(mainSource.find("TraceEntityAutoDet(pCachedSpy, vLocalShootPos, pCachedSpy->GetShootPos())"),
              std::string::npos);
    EXPECT_EQ(mainSource.find("std::vector<C_TFPlayer*> vecSpies"), std::string::npos);
}
