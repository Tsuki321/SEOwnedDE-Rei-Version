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
    EXPECT_EQ(source.find("static const int nAttributeListOffset"), std::string::npos);
    EXPECT_NE(source.find("static int nAttributeListOffset = 0"), std::string::npos);
    EXPECT_NE(source.find("static int nWeaponsOffset = 0"), std::string::npos);
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
    EXPECT_NE(header.find("TransientFailure"), std::string::npos);
    EXPECT_NE(header.find("PermanentFailure"), std::string::npos);
    EXPECT_NE(source.find("result != ApplyResult::Success"), std::string::npos);
    EXPECT_NE(source.find("m_mapAttributeDefinitions.clear()"), std::string::npos);
    EXPECT_NE(source.find("m_pItemSchema = nullptr"), std::string::npos);
    EXPECT_NE(source.find("previous.m_bEnabled || sanitized.m_bEnabled"), std::string::npos);
}

TEST(SkinChangerContracts, IsIntegratedWithRuntimeAndPersistenceLifecycle) {
    const auto root = testhelpers::FindRepoRoot();
    const auto frameStage = testhelpers::ReadTextFile(root / kFrameStageSource);
    const auto app = testhelpers::ReadTextFile(root / kAppSource);
    const auto project = testhelpers::ReadTextFile(root / kProjectSource);

    const auto postDataUpdate = frameStage.find("FRAME_NET_UPDATE_POSTDATAUPDATE_END");
    const auto run = frameStage.find("F::SkinChanger->Run()");
    const auto original = frameStage.find("CALL_ORIGINAL(ecx, curStage)");
    ASSERT_NE(postDataUpdate, std::string::npos);
    ASSERT_NE(run, std::string::npos);
    ASSERT_NE(original, std::string::npos);
    EXPECT_LT(postDataUpdate, run);
    EXPECT_LT(run, original);
    EXPECT_NE(app.find("F::SkinChanger->Load()"), std::string::npos);
    EXPECT_NE(app.find("F::SkinChanger->Save()"), std::string::npos);
    EXPECT_NE(project.find("Features\\SkinChanger\\SkinChanger.cpp"), std::string::npos);
    EXPECT_NE(project.find("Features\\SkinChanger\\SkinChanger.h"), std::string::npos);
}

TEST(SkinChangerContracts, ValidatesEveryDesiredRuntimeAttributeByIdAndValue) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSkinChangerSource);
    const auto header = testhelpers::ReadTextFile(root / kSkinChangerHeader);

    EXPECT_EQ(header.find("GetAttributeListCount"), std::string::npos);
    EXPECT_EQ(source.find("GetAttributeListCount"), std::string::npos);
    EXPECT_EQ(source.find("nCurrentAttrCount"), std::string::npos);

    const auto validatorStart = source.find("bool HasRuntimeAttributes(");
    const auto validatorEnd = source.find("int ParseItemDefinition(", validatorStart);
    ASSERT_NE(validatorStart, std::string::npos);
    ASSERT_NE(validatorEnd, std::string::npos);
    ASSERT_LT(validatorStart, validatorEnd);

    const auto validator = source.substr(validatorStart, validatorEnd - validatorStart);
    EXPECT_NE(validator.find("desiredIndex < desired.m_nCount"), std::string::npos);
    EXPECT_NE(validator.find("expected.m_Attribute"), std::string::npos);
    EXPECT_NE(validator.find("std::bit_cast<std::uint32_t>(expected.m_flValue)"),
              std::string::npos);
    EXPECT_NE(validator.find("current.m_nDefinitionIndex == nExpectedIndex"),
              std::string::npos);
    EXPECT_NE(validator.find("current.m_nRawValue == nExpectedValue"),
              std::string::npos);
    EXPECT_NE(validator.find("if (!bFound)"), std::string::npos);

    EXPECT_NE(source.find("BuildRuntimeAttributes(profile->second.m_Settings)"),
              std::string::npos);
    EXPECT_NE(source.find("HasRuntimeAttributes(pWeapon,"), std::string::npos);
}

TEST(SkinChangerContracts, EnforcesRawItemDefinitionBeforeCacheAcceptance) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSkinChangerSource);

    const auto runStart = source.find("void CSkinChanger::Run()");
    const auto runEnd = source.find("void CSkinChanger::ResetRuntimeState()", runStart);
    ASSERT_NE(runStart, std::string::npos);
    ASSERT_NE(runEnd, std::string::npos);
    ASSERT_LT(runStart, runEnd);

    const auto run = source.substr(runStart, runEnd - runStart);
    const auto rawCapture = run.find("int &nRawItemDefinition");
    const auto rawEnforcement = run.find("nRawItemDefinition = nItemDefinition", rawCapture);
    const auto cacheCheck = run.find("applied.m_nHandle", rawEnforcement);
    const auto attributeValidation = run.find("HasRuntimeAttributes", cacheCheck);
    ASSERT_NE(rawCapture, std::string::npos);
    ASSERT_NE(rawEnforcement, std::string::npos);
    ASSERT_NE(cacheCheck, std::string::npos);
    ASSERT_NE(attributeValidation, std::string::npos);
    EXPECT_LT(rawCapture, rawEnforcement);
    EXPECT_LT(rawEnforcement, cacheCheck);
    EXPECT_LT(cacheCheck, attributeValidation);
    EXPECT_NE(run.find("!bItemDefinitionReverted", cacheCheck), std::string::npos);
}

TEST(SkinChangerContracts, AdvancesAppliedStateOnlyAfterCompleteSuccess) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSkinChangerSource);

    const auto runStart = source.find("void CSkinChanger::Run()");
    const auto runEnd = source.find("void CSkinChanger::ResetRuntimeState()", runStart);
    ASSERT_NE(runStart, std::string::npos);
    ASSERT_NE(runEnd, std::string::npos);
    ASSERT_LT(runStart, runEnd);

    const auto run = source.substr(runStart, runEnd - runStart);
    const auto invalidate = run.find("applied = {}");
    const auto apply = run.find("const auto result = ApplyProfile", invalidate);
    const auto successGate = run.find("result != ApplyResult::Success", apply);
    const auto advance = run.find("applied = { handle.ToInt()", successGate);
    ASSERT_NE(invalidate, std::string::npos);
    ASSERT_NE(apply, std::string::npos);
    ASSERT_NE(successGate, std::string::npos);
    ASSERT_NE(advance, std::string::npos);
    EXPECT_LT(invalidate, apply);
    EXPECT_LT(apply, successGate);
    EXPECT_LT(successGate, advance);
    EXPECT_EQ(testhelpers::CountOccurrences(run, "applied = { handle.ToInt()"), 1u);
    EXPECT_EQ(run.find("result == ApplyResult::TransientFailure"), std::string::npos);
}

TEST(SkinChangerContracts, LoadingEmptyProfilesRefreshesPreviouslyEnabledRuntimeState) {
    const auto root = testhelpers::FindRepoRoot();
    const auto source = testhelpers::ReadTextFile(root / kSkinChangerSource);

    const auto loadStart = source.find("bool CSkinChanger::Load()");
    const auto loadEnd = source.find("bool CSkinChanger::Save()", loadStart);
    ASSERT_NE(loadStart, std::string::npos);
    ASSERT_NE(loadEnd, std::string::npos);
    ASSERT_LT(loadStart, loadEnd);

    const auto load = source.substr(loadStart, loadEnd - loadStart);
    const auto oldEnabled = load.find("const bool bHadEnabledProfiles = HasEnabledProfiles()");
    const auto swap = load.find("m_mapProfiles.swap(loadedProfiles)", oldEnabled);
    const auto refreshGate = load.find("bHadEnabledProfiles || HasEnabledProfiles()", swap);
    const auto refresh = load.find("ScheduleRuntimeRefresh()", refreshGate);
    ASSERT_NE(oldEnabled, std::string::npos);
    ASSERT_NE(swap, std::string::npos);
    ASSERT_NE(refreshGate, std::string::npos);
    ASSERT_NE(refresh, std::string::npos);
    EXPECT_LT(oldEnabled, swap);
    EXPECT_LT(swap, refreshGate);
    EXPECT_LT(refreshGate, refresh);
}

TEST(SkinChangerContracts, PreservesSkinsEditorAcrossTemporaryWeaponLoss) {
    const auto root = testhelpers::FindRepoRoot();
    const auto menu = testhelpers::ReadTextFile(root / kMenuSource);

    EXPECT_NE(menu.find(
                  "if (nCurrentWeapon >= 0 && nCurrentWeapon != m_nSkinEditorItemDefinition)"),
              std::string::npos);
    EXPECT_NE(menu.find("const int nEditorWeapon = m_nSkinEditorItemDefinition"),
              std::string::npos);
    EXPECT_NE(menu.find("SetSettings(nEditorWeapon, m_SkinEditorSettings)"),
              std::string::npos);
    EXPECT_NE(menu.find("const bool bGeoHovered = IsHoveredSimple(x, nInputY, w, h)"),
              std::string::npos);
    EXPECT_NE(menu.find(
                  "else if (H::Input->IsPressed(VK_LBUTTON) && !m_bClickConsumed && !bGeoHovered)"),
              std::string::npos);

    const auto outsideClick = menu.find(
        "else if (H::Input->IsPressed(VK_LBUTTON) && !m_bClickConsumed && !bGeoHovered)");
    ASSERT_NE(outsideClick, std::string::npos);
    const auto outsideBlock = menu.substr(outsideClick, 220);
    EXPECT_EQ(outsideBlock.find("m_bClickConsumed = true"), std::string::npos);
}
