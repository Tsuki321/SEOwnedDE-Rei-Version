#include <gtest/gtest.h>

#include "Utils/Config/Config.h"

#include <filesystem>
#include <fstream>
#include <cstdio> // remove

// Unique CFGVAR names to avoid conflicts with other test files
CFGVAR(test_cfg_bool, true);
CFGVAR(test_cfg_int, 42);
CFGVAR(test_cfg_float, 3.14f);
namespace { const Color_t test_cfg_color_init{ 100, 150, 200, 255 }; }
CFGVAR(test_cfg_color, test_cfg_color_init);
CFGVAR(test_cfg_string, std::string{ "hello_config" });

CFGVAR_NOSAVE(test_cfg_nosave_bool, false);
CFGVAR_NOSAVE(test_cfg_nosave_int, 999);

TEST(ConfigTypeDispatch, SupportedTypesUseCompactTags) {
    EXPECT_EQ(Config::GetConfigVarType<bool>(), Config::EConfigVarType::Boolean);
    EXPECT_EQ(Config::GetConfigVarType<int>(), Config::EConfigVarType::Integer);
    EXPECT_EQ(Config::GetConfigVarType<float>(), Config::EConfigVarType::Float);
    EXPECT_EQ(Config::GetConfigVarType<const Color_t&>(), Config::EConfigVarType::Color);
    EXPECT_EQ(Config::GetConfigVarType<std::string>(), Config::EConfigVarType::String);
}

// -----------------------------------------------------------------------------
// Config Save/Load Round-Trip
// -----------------------------------------------------------------------------

class ConfigTest : public ::testing::Test {
protected:
    std::filesystem::path m_tempPath;

    void SetUp() override {
        // Use a temp file name in the test directory
        m_tempPath = std::filesystem::temp_directory_path() / "seownedde_test_config.json";
    }

    void TearDown() override {
        // Clean up temp file
        std::error_code ec;
        std::filesystem::remove(m_tempPath, ec);
    }

    void EnsureCleanFile() {
        std::error_code ec;
        std::filesystem::remove(m_tempPath, ec);
    }
};

TEST_F(ConfigTest, SaveThenLoadRoundTripBool) {
    // Set a known value
    test_cfg_bool = false;
    Config::Save(m_tempPath);

    // Change the value
    test_cfg_bool = true;

    // Load back
    Config::Load(m_tempPath);

    EXPECT_FALSE(test_cfg_bool);
}

TEST_F(ConfigTest, SaveThenLoadRoundTripInt) {
    test_cfg_int = 12345;
    Config::Save(m_tempPath);

    test_cfg_int = 0;
    Config::Load(m_tempPath);

    EXPECT_EQ(test_cfg_int, 12345);
}

TEST_F(ConfigTest, SaveThenLoadRoundTripFloat) {
    test_cfg_float = 2.71828f;
    Config::Save(m_tempPath);

    test_cfg_float = 0.0f;
    Config::Load(m_tempPath);

    EXPECT_FLOAT_EQ(test_cfg_float, 2.71828f);
}

TEST_F(ConfigTest, SaveThenLoadRoundTripString) {
    test_cfg_string = "test_value_for_roundtrip";
    Config::Save(m_tempPath);

    test_cfg_string = "changed";
    Config::Load(m_tempPath);

    EXPECT_EQ(test_cfg_string, std::string("test_value_for_roundtrip"));
}

TEST_F(ConfigTest, SaveThenLoadRoundTripColor) {
    test_cfg_color = Color_t{ 10, 20, 30, 40 };
    Config::Save(m_tempPath);

    test_cfg_color = Color_t{ 0, 0, 0, 0 };
    Config::Load(m_tempPath);

    EXPECT_EQ(test_cfg_color.r, 10u);
    EXPECT_EQ(test_cfg_color.g, 20u);
    EXPECT_EQ(test_cfg_color.b, 30u);
    EXPECT_EQ(test_cfg_color.a, 40u);
}

TEST_F(ConfigTest, MalformedColorDoesNotPartiallyUpdate) {
    test_cfg_color = Color_t{ 10, 20, 30, 40 };

    nlohmann::json j;
    j["test_cfg_color"] = { 1, 2 };
    {
        std::ofstream file(m_tempPath);
        ASSERT_TRUE(file.is_open());
        file << j;
    }

    EXPECT_NO_THROW(Config::Load(m_tempPath));
    EXPECT_EQ(test_cfg_color.r, 10u);
    EXPECT_EQ(test_cfg_color.g, 20u);
    EXPECT_EQ(test_cfg_color.b, 30u);
    EXPECT_EQ(test_cfg_color.a, 40u);
}

TEST_F(ConfigTest, SaveThenLoadAllTypesAtOnce) {
    test_cfg_bool = false;
    test_cfg_int = -100;
    test_cfg_float = 0.001f;
    test_cfg_string = "multi_type_test";
    test_cfg_color = Color_t{ 200, 100, 50, 128 };

    Config::Save(m_tempPath);

    // Reset all
    test_cfg_bool = true;
    test_cfg_int = 0;
    test_cfg_float = 1.0f;
    test_cfg_string = "reset";
    test_cfg_color = Color_t{ 0, 0, 0, 0 };

    Config::Load(m_tempPath);

    EXPECT_FALSE(test_cfg_bool);
    EXPECT_EQ(test_cfg_int, -100);
    EXPECT_FLOAT_EQ(test_cfg_float, 0.001f);
    EXPECT_EQ(test_cfg_string, std::string("multi_type_test"));
    EXPECT_EQ(test_cfg_color.r, 200u);
    EXPECT_EQ(test_cfg_color.g, 100u);
    EXPECT_EQ(test_cfg_color.b, 50u);
    EXPECT_EQ(test_cfg_color.a, 128u);
}

// -----------------------------------------------------------------------------
// CFGVAR_NOSAVE — Variables Excluded from Save/Load
// -----------------------------------------------------------------------------

TEST_F(ConfigTest, NoSaveVariablesAreExcludedFromSave) {
    test_cfg_nosave_bool = true;
    test_cfg_nosave_int = 42;

    Config::Save(m_tempPath);

    // Read the file manually to check it does NOT contain the NOSAVE keys
    std::ifstream file(m_tempPath);
    ASSERT_TRUE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // The NOSAVE keys should NOT appear in the JSON file
    EXPECT_EQ(content.find("test_cfg_nosave_bool"), std::string::npos);
    EXPECT_EQ(content.find("test_cfg_nosave_int"), std::string::npos);

    // The regular keys SHOULD appear
    EXPECT_NE(content.find("test_cfg_bool"), std::string::npos);
}

TEST_F(ConfigTest, NoSaveVariablesAreNotRestoredOnLoad) {
    // Set the NOSAVE variable to a known value
    test_cfg_nosave_bool = true;

    // Save with regular variables only (NOSAVE excluded)
    Config::Save(m_tempPath);

    // Change the NOSAVE variable
    test_cfg_nosave_bool = false;

    // Load should NOT restore the NOSAVE variable
    Config::Load(m_tempPath);

    // The value should remain at whatever we set, NOT what was "saved"
    EXPECT_FALSE(test_cfg_nosave_bool);
}

// -----------------------------------------------------------------------------
// Edge Cases
// -----------------------------------------------------------------------------

TEST_F(ConfigTest, LoadingNonexistentFileDoesNothing) {
    // Make sure the file doesn't exist
    EnsureCleanFile();

    const bool before = test_cfg_bool;
    const int beforeInt = test_cfg_int;

    // Load from a non-existent path should silently return
    Config::Load(m_tempPath);

    // Values should be unchanged
    EXPECT_EQ(test_cfg_bool, before);
    EXPECT_EQ(test_cfg_int, beforeInt);
}

TEST_F(ConfigTest, MalformedJsonDoesNotCrash) {
    // Write garbage to the file
    {
        std::ofstream file(m_tempPath);
        ASSERT_TRUE(file.is_open());
        file << "this is not valid JSON {{{";
        file.close();
    }

    const bool before = test_cfg_bool;
    const int beforeInt = test_cfg_int;

    // Should not throw or crash
    EXPECT_NO_THROW(Config::Load(m_tempPath));

    // Values should be unchanged
    EXPECT_EQ(test_cfg_bool, before);
    EXPECT_EQ(test_cfg_int, beforeInt);
}

TEST_F(ConfigTest, MissingKeyInJsonIsSilentlySkipped) {
    // Save only a subset of the registered vars
    test_cfg_bool = false;
    Config::Save(m_tempPath);

    // Now manually remove one key from the JSON
    // and add an unknown key to test silent skips
    {
        // Create a JSON object with only test_cfg_bool
        nlohmann::json j;
        j["test_cfg_bool"] = false;
        j["unknown_key"] = "should_be_ignored";

        std::ofstream file(m_tempPath);
        ASSERT_TRUE(file.is_open());
        file << std::setw(4) << j;
        file.close();
    }

    // Change all values
    test_cfg_bool = true;
    test_cfg_int = 9999;

    Config::Load(m_tempPath);

    // test_cfg_bool should be restored
    EXPECT_FALSE(test_cfg_bool);
    // test_cfg_int was NOT in the JSON, so should remain changed
    EXPECT_EQ(test_cfg_int, 9999);
}

TEST_F(ConfigTest, EmptyVarsListSavesEmptyJson) {
    // Clear the vars list, save, verify empty JSON object
    // Save current vars for restoration
    const auto savedVars = Config::vars;
    Config::vars.clear();

    Config::Save(m_tempPath);

    // Read file back
    std::ifstream file(m_tempPath);
    ASSERT_TRUE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Should be empty JSON object or equivalent
    EXPECT_NE(content.find('{'), std::string::npos);

    // Restore vars
    Config::vars = savedVars;
}
