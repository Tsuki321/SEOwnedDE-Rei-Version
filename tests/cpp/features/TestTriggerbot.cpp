#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/Triggerbot.cpp";
constexpr const char* kAutoBackstabSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/AutoBackstab/AutoBackstab.cpp";
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
