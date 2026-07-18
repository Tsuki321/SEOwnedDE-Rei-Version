#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Paint";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Paint/Paint.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Paint/Paint.h";
}

TEST(PaintContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(PaintContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    const auto headerSource = testhelpers::ReadTextFile(root / kHeaderSource);
    EXPECT_NE(mainSource.find("CFG::Visuals_Paint_Active"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Draw"), std::string::npos);
    EXPECT_NE(mainSource.find("CPaint::Initialize("), std::string::npos);
    EXPECT_NE(headerSource.find("MAX_PAINT_POINTS"), std::string::npos);
    EXPECT_NE(headerSource.find("MAX_PAINT_STROKES"), std::string::npos);
    EXPECT_NE(headerSource.find("std::array<PaintRecord_t, MAX_PAINT_POINTS>"), std::string::npos);
    EXPECT_NE(headerSource.find("std::unique_ptr<PaintStorage_t>"), std::string::npos);
    EXPECT_EQ(headerSource.find("std::deque"), std::string::npos);
    EXPECT_NE(mainSource.find("CPaint::PrunePoints("), std::string::npos);
    EXPECT_NE(mainSource.find("CPaint::PopOldestPoint("), std::string::npos);
    EXPECT_NE(mainSource.find("CPaint::AddPoint("), std::string::npos);
    EXPECT_NE(mainSource.find("std::make_unique<PaintStorage_t>()"), std::string::npos);
    EXPECT_NE(mainSource.find("ClearPoints(true)"), std::string::npos);
    EXPECT_NE(mainSource.find("m_nPaintStrokeCount >= MAX_PAINT_STROKES"), std::string::npos);
    EXPECT_NE(mainSource.find("PrepareRainbowColors("), std::string::npos);
    EXPECT_NE(mainSource.find("CRenderContextScope renderContext"), std::string::npos);

    const auto activeGuard = mainSource.find("if (!CFG::Visuals_Paint_Active)");
    const auto initializeCall = mainSource.find("\tInitialize();", activeGuard);
    ASSERT_NE(activeGuard, std::string::npos);
    ASSERT_NE(initializeCall, std::string::npos);
    EXPECT_GT(initializeCall, activeGuard);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(PaintContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(4));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(1));
}
