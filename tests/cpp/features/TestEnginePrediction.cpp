#include <gtest/gtest.h>
#include "App/Features/EnginePrediction/EnginePrediction.h"

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/EnginePrediction";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/EnginePrediction/EnginePrediction.cpp";
}

TEST(EnginePredictionLifecycle, EndWithoutStartLeavesTheEngineClockAlone) {
    CGlobalVarsBase globals{};
    globals.curtime = 123.0f;
    globals.frametime = 0.015f;
    globals.tickcount = 8123;
    const auto oldGlobals = I::GlobalVars;
    const auto oldHelper = I::MoveHelper;
    I::GlobalVars = &globals;
    I::MoveHelper = nullptr;

    CEnginePrediction prediction;
    prediction.End();
    prediction.End();
    EXPECT_FLOAT_EQ(globals.curtime, 123.0f);
    EXPECT_FLOAT_EQ(globals.frametime, 0.015f);
    EXPECT_EQ(globals.tickcount, 8123);

    I::MoveHelper = oldHelper;
    I::GlobalVars = oldGlobals;
}

TEST(EnginePredictionLifecycle, SkippedStartAndResetLeaveTheClockAlone) {
    CGlobalVarsBase globals{};
    globals.curtime = 42.0f;
    globals.frametime = 0.02f;
    globals.tickcount = 2100;
    const auto oldGlobals = I::GlobalVars;
    const auto oldHelper = I::MoveHelper;
    I::GlobalVars = &globals;
    I::MoveHelper = nullptr;

    CUserCmd cmd{};
    cmd.command_number = 1;
    CEnginePrediction prediction;
    prediction.flags = FL_ONGROUND;
    prediction.Start(&cmd);
    prediction.End();
    prediction.Reset();
    prediction.Start(nullptr);
    prediction.End();
    EXPECT_EQ(prediction.flags, 0);
    EXPECT_FLOAT_EQ(globals.curtime, 42.0f);
    EXPECT_FLOAT_EQ(globals.frametime, 0.02f);
    EXPECT_EQ(globals.tickcount, 2100);

    I::MoveHelper = oldHelper;
    I::GlobalVars = oldGlobals;
}

TEST(EnginePredictionContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(EnginePredictionContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Misc_Edge_Jump_Key"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Entities"), std::string::npos);
    EXPECT_NE(mainSource.find("CEnginePrediction::GetTickbase("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(EnginePredictionContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(1));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(1));
}
