#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureDir = "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords";
constexpr const char* kMainSource = "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp";
constexpr const char* kHeaderSource = "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.h";
}

TEST(LagRecordsContracts, ContainsExpectedSourceFiles) {
    const auto root = testhelpers::FindRepoRoot();
    const auto featurePath = root / kFeatureDir;

    ASSERT_TRUE(std::filesystem::exists(featurePath));
    EXPECT_GE(testhelpers::CollectFiles(featurePath, ".cpp").size(), 1u);
    EXPECT_GE(testhelpers::CollectFiles(featurePath, ".h").size(), 1u);
}

TEST(LagRecordsContracts, UsesFixedCapacityInPlaceRing) {
    const auto root = testhelpers::FindRepoRoot();
    const auto mainSource = testhelpers::ReadTextFile(root / kMainSource);
    const auto header = testhelpers::ReadTextFile(root / kHeaderSource);

    EXPECT_NE(header.find("MAX_LAG_RECORDS"), std::string::npos);
    EXPECT_NE(header.find("std::array<std::array<LagRecord_t"), std::string::npos);
    EXPECT_NE(mainSource.find("LagRecord_t& newRecord = records[newHead]"), std::string::npos);
    EXPECT_NE(mainSource.find("m_RecordHeads[idx]"), std::string::npos);
    EXPECT_NE(mainSource.find("m_RecordCounts[idx]"), std::string::npos);
}

TEST(LagRecordsContracts, ConsumerPolicyIncludesMasterFeatureGates) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(src.find("CFG::Aimbot_Active"), std::string::npos);
    EXPECT_NE(src.find("CFG::Aimbot_Hitscan_Active"), std::string::npos);
    EXPECT_NE(src.find("CFG::Aimbot_Melee_Active"), std::string::npos);
    EXPECT_NE(src.find("CFG::Aimbot_Projectile_Active"), std::string::npos);
    EXPECT_NE(src.find("CFG::Triggerbot_Active"), std::string::npos);
    EXPECT_NE(src.find("CFG::Triggerbot_AutoBackstab_Active"), std::string::npos);
    EXPECT_NE(src.find("CFG::Materials_Active"), std::string::npos);
}

TEST(LagRecordsContracts, CapturePolicyRejectsNonEnemiesAndUnusedRecords) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(src.find("pPlayer == pLocal"), std::string::npos);
    EXPECT_NE(src.find("pPlayer->m_iTeamNum() == pLocal->m_iTeamNum()"), std::string::npos);
    EXPECT_NE(src.find("if (!AreConsumersActive())"), std::string::npos);
    EXPECT_NE(src.find("CFG::Misc_LagRecords_Skip_Offscreen"), std::string::npos);
    EXPECT_NE(src.find("F::VisualUtils->IsOnScreenNoEntity"), std::string::npos);
}

TEST(LagRecordsContracts, CapturesOnlyCoherentRenderedPlayerBody) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(src.find("pPlayer->SetupBones("), std::string::npos);
    EXPECT_NE(src.find("nBoneMask"), std::string::npos);
    EXPECT_NE(src.find("AddRenderRecord(C_TFPlayer* pPlayer, float flPoseTime)"), std::string::npos);
    EXPECT_NE(src.find("I::GlobalVars->curtime"), std::string::npos);
    EXPECT_EQ(src.find("pPlayer->InvalidateBoneCache();"), std::string::npos);

    EXPECT_EQ(src.find("FirstMoveChild"), std::string::npos);
    EXPECT_EQ(src.find("NextMovePeer"), std::string::npos);
    EXPECT_EQ(src.find("attach->SetupBones"), std::string::npos);
    EXPECT_EQ(src.find("Misc_SetupBones_Optimization"), std::string::npos);
}

TEST(LagRecordsContracts, HistoricalScopeRestoresLiveState) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMainSource);
    const auto header = testhelpers::ReadTextFile(root / kHeaderSource);

    EXPECT_NE(header.find("class CLagRecordScope"), std::string::npos);
    EXPECT_NE(header.find("bool Set(const LagRecord_t* pRecord)"), std::string::npos);
    EXPECT_NE(header.find("bool IsActive() const { return m_bActive; }"), std::string::npos);
    EXPECT_NE(header.find("static int GetCommandTick(float flPoseTime)"), std::string::npos);
    EXPECT_NE(src.find("CLagRecordMatrixHelper::CopyActiveBones"), std::string::npos);
    EXPECT_GE(testhelpers::CountOccurrences(src, "SetAbsOrigin("), 2u);
    EXPECT_GE(testhelpers::CountOccurrences(src, "SetAbsAngles("), 2u);
}

TEST(LagRecordsContracts, HistoricalBoneCopyNeverReportsPartialSuccess) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMainSource);

    EXPECT_NE(src.find("nMaxBones < nCachedCount"), std::string::npos);
    EXPECT_NE(src.find("nCachedCount != entry.BoneCount"), std::string::npos);
    EXPECT_NE(src.find("sizeof(matrix3x4_t) * nCachedCount"), std::string::npos);
}
