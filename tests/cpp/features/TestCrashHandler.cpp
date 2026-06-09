#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/CrashHandler";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/CrashHandler/CrashHandler.cpp";
constexpr const char* kHeader = "SEOwnedDE/SEOwnedDE/src/App/CrashHandler/CrashHandler.h";
constexpr const char* kDllMain = "SEOwnedDE/SEOwnedDE/src/DllMain.cpp";
}

TEST(CrashHandlerContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(1));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(1));
}

TEST(CrashHandlerContracts, MainSourceContainsSehTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;

    ASSERT_TRUE(std::filesystem::exists(mainPath));

    const auto mainSource = testhelpers::ReadTextFile(mainPath);

    // Top-level SEH registration. If this token is missing, the handler is
    // not actually wired into the unhandled-exception filter.
    EXPECT_NE(mainSource.find("SetUnhandledExceptionFilter"), std::string::npos);
    EXPECT_NE(mainSource.find("MiniDumpWriteDump"), std::string::npos);

    // User-visible error window. If MessageBoxA is missing, the handler is
    // silent on crash and the user has no way to know what happened.
    EXPECT_NE(mainSource.find("MessageBoxA"), std::string::npos);

    // Reentry guard so a crash inside the handler does not recurse.
    EXPECT_NE(mainSource.find("InterlockedCompareExchange"), std::string::npos);

    // The handler must recognize the common exception codes - access
    // violation, stack overflow, divide by zero - or the user-facing error
    // window will say "Unknown Exception" for the most common crashes.
    EXPECT_NE(mainSource.find("EXCEPTION_ACCESS_VIOLATION"), std::string::npos);
    EXPECT_NE(mainSource.find("EXCEPTION_STACK_OVERFLOW"), std::string::npos);
    EXPECT_NE(mainSource.find("EXCEPTION_INT_DIVIDE_BY_ZERO"), std::string::npos);
}

TEST(CrashHandlerContracts, HeaderExposesInstallSingleton) {
    const auto root = testhelpers::FindRepoRoot();
    const auto headerPath = root / kHeader;

    ASSERT_TRUE(std::filesystem::exists(headerPath));

    const auto header = testhelpers::ReadTextFile(headerPath);

    // Public Install() entry point.
    EXPECT_NE(header.find("Install()"), std::string::npos);

    // Singleton macro wiring so F::CrashHandler resolves in DllMain.
    EXPECT_NE(header.find("MAKE_SINGLETON_SCOPED"), std::string::npos);
    EXPECT_NE(header.find("CrashHandler, F"), std::string::npos);
}

TEST(CrashHandlerContracts, DllMainWiresHandlerOnProcessAttach) {
    const auto root = testhelpers::FindRepoRoot();
    const auto dllMainPath = root / kDllMain;

    ASSERT_TRUE(std::filesystem::exists(dllMainPath));

    const auto dllMain = testhelpers::ReadTextFile(dllMainPath);

    // Install must happen before the MainThread CreateThread, otherwise a
    // crash during App->Start() bypasses the handler. Both calls live in
    // DLL_PROCESS_ATTACH so a single DllMain block is the right scope.
    EXPECT_NE(dllMain.find("F::CrashHandler->Install"), std::string::npos);
    EXPECT_NE(dllMain.find("DLL_PROCESS_ATTACH"), std::string::npos);
    EXPECT_NE(dllMain.find("CreateThread"), std::string::npos);
}

TEST(CrashHandlerContracts, DumpPathUsesTempDir) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;

    ASSERT_TRUE(std::filesystem::exists(mainPath));

    const auto mainSource = testhelpers::ReadTextFile(mainPath);

    // %TEMP% is always writable. Falling back to a hardcoded path would
    // crash the handler on machines where that path does not exist.
    EXPECT_NE(mainSource.find("GetTempPathA"), std::string::npos);
    EXPECT_NE(mainSource.find(".dmp"), std::string::npos);
}
