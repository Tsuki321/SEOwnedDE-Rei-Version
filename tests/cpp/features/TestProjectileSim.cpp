#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/ProjectileSim";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/ProjectileSim/ProjectileSim.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/ProjectileSim/ProjectileSim.h";
constexpr const char* kAppSource = "SEOwnedDE/SEOwnedDE/src/App/App.cpp";
constexpr const char* kDllMainSource = "SEOwnedDE/SEOwnedDE/src/DllMain.cpp";
}

TEST(ProjectileSimContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(ProjectileSimContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CProjectileSim::GetInfo("), std::string::npos);
}

TEST(ProjectileSimContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(1));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(2));
}

TEST(ProjectileSimContracts, ReleasesPhysicsBeforeDllDetach) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);
    const auto headerSource = testhelpers::ReadTextFile(root / kHeaderSource);
    const auto appSource = testhelpers::ReadTextFile(root / kAppSource);
    const auto dllMainSource = testhelpers::ReadTextFile(root / kDllMainSource);

    EXPECT_NE(headerSource.find("void CleanUp();"), std::string::npos);
    EXPECT_EQ(headerSource.find("~CProjectileSim"), std::string::npos);
    EXPECT_EQ(mainSource.find("CProjectileSim::~CProjectileSim"), std::string::npos);

    const auto cleanupStart = mainSource.find("void CProjectileSim::CleanUp()");
    const auto cleanupEnd = mainSource.find("bool CProjectileSim::GetInfo", cleanupStart);
    ASSERT_NE(cleanupStart, std::string::npos);
    ASSERT_NE(cleanupEnd, std::string::npos);

    const auto cleanupBody = mainSource.substr(cleanupStart, cleanupEnd - cleanupStart);
    EXPECT_NE(cleanupBody.find("DestroyObject"), std::string::npos);
    EXPECT_NE(cleanupBody.find("DestroyCollide"), std::string::npos);
    EXPECT_NE(cleanupBody.find("DestroyEnvironment"), std::string::npos);
    EXPECT_NE(cleanupBody.find("m_pObj = nullptr;"), std::string::npos);
    EXPECT_NE(cleanupBody.find("m_pCollide = nullptr;"), std::string::npos);
    EXPECT_NE(cleanupBody.find("m_pEnv = nullptr;"), std::string::npos);
    EXPECT_EQ(testhelpers::CountOccurrences(mainSource, "DestroyObject"),
              testhelpers::CountOccurrences(cleanupBody, "DestroyObject"));
    EXPECT_EQ(testhelpers::CountOccurrences(mainSource, "DestroyCollide"),
              testhelpers::CountOccurrences(cleanupBody, "DestroyCollide"));
    EXPECT_EQ(testhelpers::CountOccurrences(mainSource, "DestroyEnvironment"),
              testhelpers::CountOccurrences(cleanupBody, "DestroyEnvironment"));

    const auto shutdownStart = appSource.find("void CApp::Shutdown()");
    ASSERT_NE(shutdownStart, std::string::npos);
    const auto shutdownBody = appSource.substr(shutdownStart);
    EXPECT_NE(shutdownBody.find("F::ProjectileSim->CleanUp();"), std::string::npos);

    const auto shutdownCall = dllMainSource.find("App->Shutdown();");
    const auto freeLibraryCall = dllMainSource.find("FreeLibraryAndExitThread");
    ASSERT_NE(shutdownCall, std::string::npos);
    ASSERT_NE(freeLibraryCall, std::string::npos);
    EXPECT_LT(shutdownCall, freeLibraryCall);
}
