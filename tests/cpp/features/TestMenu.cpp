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

TEST(MenuContracts, HitscanAssistHasAnIndependentToggleAndOnlyThreeAimModes) {
    const auto source = testhelpers::ReadTextFile(testhelpers::FindRepoRoot() / kMainSource);
    EXPECT_NE(source.find("CheckBox(\"Aim Assist\", CFG::Aimbot_Hitscan_AimAssist_Active)"), std::string::npos);
    EXPECT_EQ(source.find("{ \"Aim Assist\", 3 }"), std::string::npos);
    const auto hitscan = source.find("GroupBoxStart(\"Hitscan\"");
    ASSERT_NE(hitscan, std::string::npos);
    const auto mode = source.find("SelectSingle(\"Aim Type\", CFG::Aimbot_Hitscan_Aim_Type", hitscan);
    ASSERT_NE(mode, std::string::npos);
    const auto choices = source.substr(mode, source.find("});", mode) - mode);
    EXPECT_NE(choices.find("{ \"Plain\", 0 }"), std::string::npos);
    EXPECT_NE(choices.find("{ \"Silent\", 1 }"), std::string::npos);
    EXPECT_NE(choices.find("{ \"Smooth\", 2 }"), std::string::npos);
}

TEST(MenuLayout, TallPagesGrowTheWindowButRemainOnSmallScreens) {
    EXPECT_EQ(MenuLayout::WindowHeight(420, 760, 1080, 8), 760);
    EXPECT_EQ(MenuLayout::WindowHeight(420, 760, 600, 8), 584);
    EXPECT_EQ(MenuLayout::WindowHeight(420, 120, 1080, 8), 420);
    EXPECT_EQ(MenuLayout::WindowHeight(420, 760, 12, 8), 1);
}

TEST(MenuLayout, ScrollingReachesTheLastRowAndClampsWhenThePageShrinks) {
    EXPECT_EQ(MenuLayout::ClampScroll(1000, 900, 500), 400);
    EXPECT_EQ(900 - MenuLayout::ClampScroll(1000, 900, 500), 500);
    EXPECT_EQ(MenuLayout::ClampScroll(-20, 900, 500), 0);
    EXPECT_EQ(MenuLayout::ClampScroll(400, 250, 500), 0);
    EXPECT_EQ(MenuLayout::ScrollLimit(900, 0), 900);
}

TEST(MenuLayout, PopupsMoveUpToKeepAllRowsInsideTheViewport) {
    const MenuLayout::Rect viewport{ 200, 100, 500, 420 };
    EXPECT_EQ(MenuLayout::PopupTop(480, 120, viewport), 400);
    EXPECT_EQ(MenuLayout::PopupTop(60, 120, viewport), 100);
    EXPECT_EQ(MenuLayout::PopupTop(200, 120, viewport), 200);
    EXPECT_FALSE(MenuLayout::Contains(viewport, 250, 99));
    EXPECT_FALSE(MenuLayout::Contains(viewport, 700, 200));
    EXPECT_TRUE(MenuLayout::Contains(viewport, 200, 100));
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
    bool clippingDisabled = true;
    std::vector<std::array<int, 4>> printedClips;

    static void __fastcall SetFont(Instance*, HFont) {}
    static void __fastcall SetColor(Instance*, int, int, int, int) {}
    static void __fastcall SetColorValue(Instance*, Color_t) {}
    static void __fastcall SetPosition(Instance*, int, int) {}
    static void __fastcall Print(Instance* self, const wchar_t* text, int size, FontDrawType_t) {
        self->owner->text.emplace_back(text, size);
        self->owner->printedClips.push_back(self->owner->clip);
    }
    static void __fastcall DisableClipping(Instance* self, bool disabled) {
        self->owner->clipping.push_back(disabled);
        self->owner->clippingDisabled = disabled;
    }
    static void __fastcall GetClip(Instance* self, int& x, int& y, int& right, int& bottom, bool& disabled) {
        const auto& clip = self->owner->clip;
        x = clip[0]; y = clip[1]; right = clip[2]; bottom = clip[3];
        disabled = self->owner->clippingDisabled;
    }
    static void __fastcall SetClip(Instance* self, int x, int y, int right, int bottom) {
        self->owner->clip = { x, y, right, bottom };
    }
    TextSurface() {
        functions[17] = reinterpret_cast<uintptr_t>(&SetFont);
        functions[18] = reinterpret_cast<uintptr_t>(&SetColorValue);
        // MSVC places overloaded virtual methods in reverse declaration order.
        functions[19] = reinterpret_cast<uintptr_t>(&SetColor);
        functions[20] = reinterpret_cast<uintptr_t>(&SetPosition);
        functions[22] = reinterpret_cast<uintptr_t>(&Print);
        functions[156] = reinterpret_cast<uintptr_t>(&DisableClipping);
        functions[157] = reinterpret_cast<uintptr_t>(&GetClip);
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
    ASSERT_EQ(surface.printedClips.size(), 1u);
    EXPECT_EQ(surface.printedClips[0], (std::array<int, 4>{ 0, 12, 100, 32 }));
    EXPECT_EQ(surface.clip, (std::array<int, 4>{ 0, 0, 0, 0 }));
    EXPECT_TRUE(surface.clippingDisabled);
}

TEST_F(DeferredTextTest, NestedTextAndWindowClipsRestoreTheExistingGameClip) {
    surface.clip = { 0, 0, 80, 80 };
    surface.clippingDisabled = false;
    CLateRenderer renderer;
    renderer.SetClip({ 20, 20, 100, 100 });
    renderer.String(font, 20, 30, Colors::WHITE, POS_DEFAULT, "bounded", { 10, 30, 100, 20 });
    renderer.String(font, 20, 60, Colors::WHITE, POS_DEFAULT, "next", {});
    renderer.DrawAll();
    ASSERT_EQ(surface.printedClips.size(), 2u);
    EXPECT_EQ(surface.printedClips[0], (std::array<int, 4>{ 20, 30, 80, 50 }));
    EXPECT_EQ(surface.printedClips[1], (std::array<int, 4>{ 20, 20, 80, 80 }));
    EXPECT_EQ(surface.clip, (std::array<int, 4>{ 0, 0, 80, 80 }));
    EXPECT_FALSE(surface.clippingDisabled);
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
