#include <gtest/gtest.h>

#include "../helpers/SourceContractAssertions.h"

namespace {
constexpr const char* kFeatureCpp =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.cpp";
constexpr const char* kFeatureHeader =
    "SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/LagRecords.h";
}

// Extra lag-record contracts that complement TestLagRecords.cpp:
// - rotational + feet-yaw capture
// - sim-time validity window
// - bone matrix swap helper symmetry
// - failed wearable child tracking (Phase 2)

TEST(LagRecordsRotationalContracts, RecordCapturesAnglesAndFeetYaw) {
    const auto root = testhelpers::FindRepoRoot();
    const auto headerPath = root / kFeatureHeader;
    const auto cppPath = root / kFeatureCpp;

    ASSERT_TRUE(std::filesystem::exists(headerPath));
    ASSERT_TRUE(std::filesystem::exists(cppPath));

    const auto header = testhelpers::ReadTextFile(headerPath);
    const auto src = testhelpers::ReadTextFile(cppPath);

    // LagRecord_t struct fields used by Phase 2 bone-fidelity work.
    EXPECT_NE(header.find("AbsAngles"), std::string::npos);
    EXPECT_NE(header.find("AbsOrigin"), std::string::npos);
    EXPECT_NE(header.find("FeetYaw"), std::string::npos);
    EXPECT_NE(header.find("BoneMatrix[MAX_BONE_COUNT]"), std::string::npos);

    // AddRecord captures live values into the record.
    EXPECT_NE(src.find("newRecord.AbsAngles"), std::string::npos);
    EXPECT_NE(src.find("newRecord.AbsOrigin"), std::string::npos);
    EXPECT_NE(src.find("newRecord.FeetYaw"), std::string::npos);
    EXPECT_NE(src.find("m_flCurrentFeetYaw"), std::string::npos);
}

TEST(LagRecordsRotationalContracts, SimulationTimeWindowIsConfigurable) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    EXPECT_NE(src.find("IsSimulationTimeValid"), std::string::npos);

    EXPECT_NE(src.find("LagRecords_BacktrackWindow"), std::string::npos);

    EXPECT_NE(src.find("sv_maxunlag"), std::string::npos);
    EXPECT_NE(src.find("GetFloat"), std::string::npos);
}

TEST(LagRecordsRotationalContracts, BoneMatrixHelperPreservesAndRestores) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    // Set() must save the current pose before swapping in the recorded one.
    EXPECT_NE(src.find("CLagRecordMatrixHelper::Set"), std::string::npos);
    EXPECT_NE(src.find("CLagRecordMatrixHelper::Restore"), std::string::npos);

    // Symmetry: SetAbsOrigin / SetAbsAngles applied in both Set and Restore.
    EXPECT_GE(testhelpers::CountOccurrences(src, "SetAbsOrigin("), 2u);
    EXPECT_GE(testhelpers::CountOccurrences(src, "SetAbsAngles("), 2u);

    // Stack-based helper: depth tracking prevents Restore() from running when empty.
    EXPECT_NE(src.find("m_nActiveDepth"), std::string::npos);
}

TEST(LagRecordsRotationalContracts, DiffersFromCurrentChecksOriginAnglesFlagsAndFeetYaw) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    EXPECT_NE(src.find("DiffersFromCurrent"), std::string::npos);
    EXPECT_NE(src.find("DiffersFromCurrentCached"), std::string::npos);
    EXPECT_NE(src.find("CacheCurrentState"), std::string::npos);
    EXPECT_NE(src.find("pRecord->AbsOrigin"), std::string::npos);
    EXPECT_NE(src.find("pRecord->EyeAngles"), std::string::npos);
    EXPECT_NE(src.find("pRecord->Flags"), std::string::npos);
    EXPECT_NE(src.find("pRecord->FeetYaw"), std::string::npos);
}

// Phase 2: failed wearable-child SetupBones tracking. The set must exist in the
// header and be referenced from both AddRecord (insertion / per-player clear) and
// the SetupBones cache short-circuit.
TEST(LagRecordsRotationalContracts, FailedChildBonesSetIsDeclaredAndUsed) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kFeatureHeader);
    const auto cpp = testhelpers::ReadTextFile(root / kFeatureCpp);

    EXPECT_NE(header.find("m_FailedChildBones"), std::string::npos)
        << "Phase 2 expects CLagRecords::m_FailedChildBones to be declared.";
    EXPECT_NE(header.find("HasFailedBones"), std::string::npos)
        << "Phase 2 expects HasFailedBones() accessor.";
    EXPECT_NE(cpp.find("m_FailedChildBones"), std::string::npos);
}
