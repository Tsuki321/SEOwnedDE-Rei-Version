#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kCfgHeader = "SEOwnedDE/SEOwnedDE/src/App/Features/CFG.h";
}

TEST(CFGContracts, HeaderContainsCoreFeatureToggles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cfgPath = root / kCfgHeader;

    ASSERT_TRUE(std::filesystem::exists(cfgPath));

    const auto text = testhelpers::ReadTextFile(cfgPath);

    EXPECT_NE(text.find("Aimbot_Active"), std::string::npos);
    EXPECT_NE(text.find("Triggerbot_Active"), std::string::npos);
    EXPECT_NE(text.find("ESP_Active"), std::string::npos);
    EXPECT_NE(text.find("Visuals_World_Modulation_Mode"), std::string::npos);
    EXPECT_NE(text.find("Misc_Accuracy_Improvements"), std::string::npos);
}

TEST(CFGContracts, HeaderHasWideConfigurationSurface) {
    const auto root = testhelpers::FindRepoRoot();
    const auto text = testhelpers::ReadTextFile(root / kCfgHeader);

    EXPECT_GE(testhelpers::CountOccurrences(text, "CFGVAR("), 300u);
    EXPECT_GE(testhelpers::CountOccurrences(text, "Color_t"), 10u);
    EXPECT_GE(testhelpers::CountOccurrences(text, "#pragma region"), 8u);
}
