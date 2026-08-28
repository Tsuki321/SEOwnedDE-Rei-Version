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
// - explicit historical-scope isolation from live rendering

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

    // AddRenderRecord captures coherent rendered values into the record.
    EXPECT_NE(src.find("newRecord.AbsAngles"), std::string::npos);
    EXPECT_NE(src.find("newRecord.AbsOrigin"), std::string::npos);
    EXPECT_NE(src.find("newRecord.FeetYaw"), std::string::npos);
    EXPECT_NE(src.find("m_flCurrentFeetYaw"), std::string::npos);
}

TEST(LagRecordsRotationalContracts, SimulationTimeWindowUsesServerConVar) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    EXPECT_NE(src.find("IsSimulationTimeValid"), std::string::npos);

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

// Restore() bail-outs must not leave backtracked bones installed at the live
// origin. Whenever the saved matrices cannot be safely handed back - missing
// cached bone data, an implausible Count(), a null Base(), or a bone count
// that moved between Set and Restore - the player's bone cache is invalidated
// so the engine rebuilds from the restored origin instead of serving the
// stretched half-record skeleton those stale bones would produce.
TEST(LagRecordsRotationalContracts, RestoreBailOutsInvalidateTheBoneCache) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    // One invalidation per bail-out path (null data / bad count / null base /
    // count mismatch), all addressed through the stack entry's player.
    EXPECT_GE(testhelpers::CountOccurrences(src, "entry.Player->InvalidateBoneCache();"), 4u);
    EXPECT_NE(src.find("nCachedRestore != entry.BoneCount"), std::string::npos);

    // Capture still must not invalidate: records are copied out of the
    // engine-owned pose without disturbing it.
    EXPECT_EQ(src.find("pPlayer->InvalidateBoneCache();"), std::string::npos);
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

TEST(LagRecordsRotationalContracts, HistoricalCopyIsScopedToActivePlayer) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kFeatureHeader);
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    EXPECT_NE(header.find("CopyActiveBones"), std::string::npos);
    EXPECT_NE(header.find("AppliedBoneCount"), std::string::npos);
    EXPECT_NE(src.find("entry.Player != pEntity"), std::string::npos);
    EXPECT_NE(src.find("entry.AppliedBoneCount"), std::string::npos);
	EXPECT_NE(src.find("nMaxBones < nCachedCount"), std::string::npos);
    EXPECT_EQ(header.find("m_FailedChildBones"), std::string::npos);
    EXPECT_EQ(src.find("FirstMoveChild"), std::string::npos);
}

// Velocity/tick-relative teleport detection: the fixed 64u gate must be scaled
// by elapsed time and the recorded velocity so multi-tick / choke gaps and fast
// movers are not misclassified as teleports (dropping otherwise-valid records).
TEST(LagRecordsRotationalContracts, TeleportDetectionScalesWithVelocityAndTime) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kFeatureHeader);
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    // New tuning constants declared in the header.
    EXPECT_NE(header.find("LAG_COMPENSATION_TELEPORTED_BASE_RADIUS"), std::string::npos);
    EXPECT_NE(header.find("LAG_COMPENSATION_TELEPORTED_VELOCITY_SLACK"), std::string::npos);

    // AddRenderRecord derives an allowance from velocity * dt before flagging.
    EXPECT_NE(src.find("head.Velocity.Length()"), std::string::npos);
    EXPECT_NE(src.find("flSimTime - head.SimulationTime"), std::string::npos);
    EXPECT_NE(src.find("bTeleported = true"), std::string::npos);
}

// Shared usability predicate: the null + teleport + differs filter is defined
// once on CLagRecords and reused by every backtrack consumer.
TEST(LagRecordsRotationalContracts, IsRecordUsablePredicateIsCentralized) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kFeatureHeader);
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    EXPECT_NE(header.find("IsRecordUsable"), std::string::npos);
    EXPECT_NE(src.find("bool CLagRecords::IsRecordUsable("), std::string::npos);

    // Predicate composes the teleport gate and the differs-from-live check.
    EXPECT_NE(src.find("pRecord->bTeleported"), std::string::npos);
    EXPECT_NE(src.find("DiffersFromCurrentCached(pRecord, cached)"), std::string::npos);
}

// GetCachedState must reject out-of-range indices (hostile / recycled entindex)
// instead of indexing m_CachedStates out of bounds.
TEST(LagRecordsRotationalContracts, GetCachedStateBoundsChecksIndex) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kFeatureHeader);

    EXPECT_NE(header.find("GetCachedState"), std::string::npos);
    // Guard clause on the valid [1, MAX_PLAYERS) range.
    EXPECT_NE(header.find("nPlayerIndex < 1 || nPlayerIndex >= MAX_PLAYERS"), std::string::npos);
}

// Unstable-ping hardening: the latency term that scopes the validity window is
// exponentially smoothed so a connection alternating high/low ping does not
// thrash the window and destructively truncate deep records on low-ping frames.
TEST(LagRecordsRotationalContracts, ValidityWindowLatencyIsSmoothed) {
    const auto root = testhelpers::FindRepoRoot();
    const auto header = testhelpers::ReadTextFile(root / kFeatureHeader);
    const auto src = testhelpers::ReadTextFile(root / kFeatureCpp);

    EXPECT_NE(header.find("LAG_LATENCY_EMA_ALPHA"), std::string::npos);
    EXPECT_NE(header.find("m_flSmoothedLatency"), std::string::npos);
    // EMA update feeds the window latency instead of the raw sample.
    EXPECT_NE(src.find("m_flSmoothedLatency += (flRawLatency - m_flSmoothedLatency) * LAG_LATENCY_EMA_ALPHA"), std::string::npos);
    EXPECT_NE(src.find("const float flLatency = m_flSmoothedLatency + SDKUtils::GetLerp()"), std::string::npos);
    // Reset to re-seed on a full-ring clear (death / ghost / kart).
    EXPECT_NE(src.find("m_flSmoothedLatency = -1.0f"), std::string::npos);
}
