#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kSkinChangerSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/SkinChanger/SkinChanger.cpp";
constexpr const char* kSkinChangerHeader =
    "SEOwnedDE/SEOwnedDE/src/App/Features/SkinChanger/SkinChanger.h";
constexpr const char* kMenuSource =
    "SEOwnedDE/SEOwnedDE/src/App/Features/Menu/Menu.cpp";
constexpr const char* kFrameStageSource =
    "SEOwnedDE/SEOwnedDE/src/App/Hooks/IBaseClientDLL_FrameStageNotify.cpp";
constexpr const char* kAppSource = "SEOwnedDE/SEOwnedDE/src/App/App.cpp";
constexpr const char* kProjectSource = "SEOwnedDE/SEOwnedDE/SEOwnedDE.vcxproj";
}

TEST(SkinChangerContracts, UsesIdaVerifiedSignaturesAndDynamicAttributeLayout) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSkinChangerSource);

    EXPECT_NE(source.find("48 83 EC 28 E8 ? ? ? ? 48 83 C0 08"), std::string::npos);
    EXPECT_NE(source.find(
                  "89 54 24 ? 53 48 83 EC 20 48 8B D9 48 8D 54 24 ? "
                  "48 81 C1 50 02 00 00"),
              std::string::npos);
    EXPECT_NE(source.find(
                  "48 89 5C 24 ? 55 56 57 48 8B EC 48 83 EC 50 44 8B 49"),
              std::string::npos);
    EXPECT_NE(source.find("NetVars::GetNetVar(\"CEconEntity\", \"m_AttributeList\")"),
              std::string::npos);
    EXPECT_EQ(source.find("0xDB8"), std::string::npos);
}

TEST(SkinChangerContracts, RunsInTheNativeMenuWithoutAWebController) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSkinChangerSource);
    const auto menu = testhelpers::ReadTextFile(root / kMenuSource);
    const auto project = testhelpers::ReadTextFile(root / kProjectSource);

    EXPECT_EQ(source.find("httplib"), std::string::npos);
    EXPECT_EQ(source.find("WebInterface"), std::string::npos);
    EXPECT_EQ(source.find("listen("), std::string::npos);
    EXPECT_EQ(project.find("httplib"), std::string::npos);
    EXPECT_EQ(project.find("WebInterface"), std::string::npos);
    EXPECT_NE(menu.find("EMainTabs::SKINS"), std::string::npos);
    EXPECT_NE(menu.find("Button(\"Skins\""), std::string::npos);
    EXPECT_NE(menu.find("InputInt(\"Paint Kit\""), std::string::npos);
    EXPECT_NE(menu.find("SelectSingle(\"Sheen\""), std::string::npos);
}

TEST(SkinChangerContracts, DebouncesRefreshAndCachesAppliedWeapons) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSkinChangerSource);
    const auto header = testhelpers::ReadTextFile(root / kSkinChangerHeader);

    EXPECT_NE(source.find("kRuntimeRefreshDelay"), std::string::npos);
    EXPECT_NE(source.find("kSaveDelay"), std::string::npos);
    EXPECT_NE(source.find("HasEnabledProfiles()"), std::string::npos);
    EXPECT_NE(source.find("std::array<RuntimeAttribute, kMaxRuntimeAttributes>"),
              std::string::npos);
    EXPECT_NE(source.find("m_nDeltaTick = -1"), std::string::npos);
    EXPECT_NE(header.find("m_arrAppliedWeapons"), std::string::npos);
    EXPECT_NE(header.find("m_nRevision"), std::string::npos);
}

TEST(SkinChangerContracts, IsIntegratedWithRuntimeAndPersistenceLifecycle) {
    const auto root = testhelpers::FindRepoRoot();
    const auto frameStage = testhelpers::ReadTextFile(root / kFrameStageSource);
    const auto app = testhelpers::ReadTextFile(root / kAppSource);
    const auto project = testhelpers::ReadTextFile(root / kProjectSource);

    EXPECT_NE(frameStage.find("FRAME_NET_UPDATE_POSTDATAUPDATE_END"), std::string::npos);
    EXPECT_NE(frameStage.find("F::SkinChanger->Run()"), std::string::npos);
    EXPECT_NE(app.find("F::SkinChanger->Load()"), std::string::npos);
    EXPECT_NE(app.find("F::SkinChanger->Save()"), std::string::npos);
    EXPECT_NE(project.find("Features\\SkinChanger\\SkinChanger.cpp"), std::string::npos);
    EXPECT_NE(project.find("Features\\SkinChanger\\SkinChanger.h"), std::string::npos);
}
