#include <gtest/gtest.h>
#include "App/Features/Menu/LateRenderer/LateRenderer.h"
#include <limits>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu/Menu.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Menu/Menu.h";
}

TEST(MenuContracts, AvoidsPerFrameModalVectorAndFilesystemChurn) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);
    const auto headerSource = testhelpers::ReadTextFile(root / kHeaderSource);

    EXPECT_NE(headerSource.find("m_pActiveControl"), std::string::npos);
    EXPECT_EQ(headerSource.find("m_mapStates"), std::string::npos);
    EXPECT_NE(headerSource.find("std::initializer_list<std::pair<const char *, int>>"), std::string::npos);
    EXPECT_NE(mainSource.find("flNextConfigRefresh"), std::string::npos);
    EXPECT_NE(mainSource.find("Plat_FloatTime()"), std::string::npos);
    EXPECT_NE(mainSource.find("MAX_SNOW_FLAKES = 400"), std::string::npos);

    const auto openGuard = mainSource.find("if (m_bOpen)");
    const auto gradientAllocation = mainSource.find("m_pGradient = std::make_unique", openGuard);
    ASSERT_NE(openGuard, std::string::npos);
    ASSERT_NE(gradientAllocation, std::string::npos);
    EXPECT_GT(gradientAllocation, openGuard);
}

TEST(MenuContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));

    const auto cppFiles = testhelpers::CollectFiles(featurePath, ".cpp");
    const auto headerFiles = testhelpers::CollectFiles(featurePath, ".h");

    EXPECT_GE(cppFiles.size(), static_cast<std::size_t>(2));
    EXPECT_GE(headerFiles.size(), static_cast<std::size_t>(2));
}

TEST(MenuContracts, MainSourceContainsFeatureTokens) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_TRUE(std::filesystem::exists(mainPath));
    ASSERT_FALSE(cppFiles.empty());

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CFG::Menu_Spacing_Y"), std::string::npos);
    EXPECT_NE(mainSource.find("H::Input"), std::string::npos);
    EXPECT_NE(mainSource.find("CMenu::Drag("), std::string::npos);
    EXPECT_GE(testhelpers::CountTokenAcrossFiles(cppFiles, "CFG::"), 1u);
}

TEST(MenuContracts, UsesGuardClausesAndReturns) {
    const auto root = testhelpers::FindRepoRoot();
    const auto cppFiles = testhelpers::CollectFiles(root / kFeatureDir, ".cpp");

    ASSERT_FALSE(cppFiles.empty());

    const auto totalIfs = testhelpers::CountTokenAcrossFiles(cppFiles, "if (");
    const auto totalReturns = testhelpers::CountTokenAcrossFiles(cppFiles, "return");

    EXPECT_GE(totalIfs, static_cast<std::size_t>(28));
    EXPECT_GE(totalReturns, static_cast<std::size_t>(9));
}

TEST(MenuContracts, ProjectilePredictionMethodShowsRelevantTuningControlsOnly) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainPath = root / kMainSource;

    ASSERT_TRUE(std::filesystem::exists(mainPath));

    const auto mainSource = testhelpers::ReadTextFile(mainPath);
    EXPECT_NE(mainSource.find("CheckBox(\"High Arc\", CFG::Aimbot_Projectile_High_Arc)"), std::string::npos);
    EXPECT_NE(mainSource.find("SelectSingle(\"Prediction Method\", CFG::Aimbot_Projectile_Aim_Prediction_Method"), std::string::npos);
    EXPECT_NE(mainSource.find("{ \"Constant Velocity\", 0 }"), std::string::npos);
    EXPECT_NE(mainSource.find("{ \"Acceleration Tracking\", 1 }"), std::string::npos);
    EXPECT_EQ(mainSource.find("Aimbot_Projectile_Aim_Prediction_Method == 4"), std::string::npos);
    EXPECT_EQ(mainSource.find("Aimbot_Projectile_Aim_Prediction_Method == 5"), std::string::npos);
}

namespace {
// Headless implementation of only the SDK surface slots used by text drawing.
// The slot indices include IAppSystem and ISurface's Shutdown override.
struct TextSurface {
    std::array<uintptr_t, 180> functions{};
    struct Instance {
        uintptr_t* table;
        TextSurface* owner;
    } instance{ functions.data(), this };
    std::vector<std::wstring> text;
    std::vector<bool> clipping;
    std::array<int, 4> clip{};

    static void __fastcall SetFont(Instance*, HFont) {}
    static void __fastcall SetColor(Instance*, int, int, int, int) {}
    static void __fastcall SetPosition(Instance*, int, int) {}
    static void __fastcall Print(Instance* self, const wchar_t* text, int size, FontDrawType_t) {
        self->owner->text.emplace_back(text, size);
    }
    static void __fastcall DisableClipping(Instance* self, bool disabled) {
        self->owner->clipping.push_back(disabled);
    }
    static void __fastcall SetClip(Instance* self, int x, int y, int right, int bottom) {
        self->owner->clip = { x, y, right, bottom };
    }
    TextSurface() {
        functions[17] = reinterpret_cast<uintptr_t>(&SetFont);
        functions[18] = reinterpret_cast<uintptr_t>(&SetColor);
        // MSVC places overloaded virtual methods in reverse declaration order.
        // Both color overloads are harmless no-ops in this text-only surface.
        functions[19] = reinterpret_cast<uintptr_t>(&SetColor);
        functions[20] = reinterpret_cast<uintptr_t>(&SetPosition);
        functions[22] = reinterpret_cast<uintptr_t>(&Print);
        functions[156] = reinterpret_cast<uintptr_t>(&DisableClipping);
        functions[158] = reinterpret_cast<uintptr_t>(&SetClip);
    }
    IMatSystemSurface* Get() { return reinterpret_cast<IMatSystemSurface*>(&instance); }
};

class DeferredTextTest : public ::testing::Test {
protected:
    TextSurface surface;
    IMatSystemSurface* previousSurface = nullptr;
    CFont font{ "test", 12, 0, 400, 1 };
    void SetUp() override {
        previousSurface = I::MatSystemSurface;
        I::MatSystemSurface = surface.Get();
    }
    void TearDown() override { I::MatSystemSurface = previousSurface; }
};
}

TEST_F(DeferredTextTest, QueueOwnsTemporaryTextAndDisplaysPercentCharactersLiterally) {
    CLateRenderer renderer;
    std::string name = "player %s %n 100%";
    renderer.String(font, 0, 0, Colors::WHITE, POS_DEFAULT, name.c_str(), {});
    name.assign(name.size(), 'x');
    renderer.String(font, 0, 0, Colors::WHITE, POS_DEFAULT, std::string("temporary").c_str(), {});
    renderer.DrawAll();
    ASSERT_EQ(surface.text.size(), 2u);
    EXPECT_EQ(surface.text[0], L"player %s %n 100%");
    EXPECT_EQ(surface.text[1], L"temporary");
}

TEST_F(DeferredTextTest, ClipAtScreenEdgeIsEnabledAndThenReleased) {
    CLateRenderer renderer;
    renderer.String(font, 0, 0, Colors::WHITE, POS_DEFAULT, "edge", { 0, 12, 100, 20 });
    renderer.DrawAll();
    ASSERT_EQ(surface.clipping.size(), 2u);
    EXPECT_FALSE(surface.clipping[0]);
    EXPECT_TRUE(surface.clipping[1]);
    EXPECT_EQ(surface.clip, (std::array<int, 4>{ 0, 12, 100, 32 }));
}

TEST_F(DeferredTextTest, NullAndClearedStringsAreNotDrawn) {
    CLateRenderer renderer;
    renderer.String(font, 0, 0, Colors::WHITE, POS_DEFAULT, nullptr, {});
    renderer.String(font, 0, 0, Colors::WHITE, POS_DEFAULT, "discarded", {});
    renderer.Clear();
    renderer.DrawAll();
    EXPECT_TRUE(surface.text.empty());
}

TEST_F(DeferredTextTest, LongNarrowAndWideTextTruncateWithoutCrashing) {
    CDraw draw;
    const std::string narrow(4096, 'a');
    const std::wstring wide(4096, L'b');
    draw.String(font, 0, 0, Colors::WHITE, POS_DEFAULT, "%s", narrow.c_str());
    draw.String(font, 0, 0, Colors::WHITE, POS_DEFAULT, L"%ls", wide.c_str());
    ASSERT_EQ(surface.text.size(), 2u);
    EXPECT_EQ(surface.text[0], std::wstring(1023, L'a'));
    EXPECT_EQ(surface.text[1], std::wstring(1023, L'b'));
}

TEST(InputSafety, InvalidConfiguredKeysNeverIndexTheKeyboardState) {
    CInput input;
    for (const int key : { -1, -32768, 256, 32767, 65537, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() }) {
        EXPECT_FALSE(input.IsPressed(key));
        EXPECT_FALSE(input.IsHeld(key));
        EXPECT_FALSE(input.IsDown(key));
        EXPECT_FALSE(input.IsPressedAndHeld(key));
    }
    for (const int key : { 0, 1, 255 }) {
        EXPECT_FALSE(input.IsPressed(key));
        EXPECT_FALSE(input.IsHeld(key));
    }
}
