#include <gtest/gtest.h>

#include "App/Features/LagRecords/LagRecords.h"

#include "../helpers/SourceContractAssertions.h"

// Unlike the rest of the LagRecords tests, most of this file EXECUTES the real
// filter instead of grepping the source for it. GetRecordAge,
// IsWithinBacktrackWindow and DiffersFromCurrentCached are static and touch only
// plain fields of LagRecord_t / LagRecordCachedState_t, so they run headless with
// no game, no interfaces and no live C_TFPlayer. That covers the regression this
// suite previously could not see at all: a record being offered to a shot even
// though the server would refuse to rewind that far.
//
// What still has to be asserted as source text is called out where it appears.

namespace {

// A record whose pose is identical to the neutral cached state below, so the only
// thing under test is its age.
void MakeRecord(LagRecord_t& record, float flPoseTime)
{
	record.Player = nullptr;
	record.SimulationTime = flPoseTime;
	record.AbsOrigin = Vec3(0.0f, 0.0f, 0.0f);
	record.EyeAngles = Vec3(0.0f, 0.0f, 0.0f);
	record.Flags = 0;
	record.FeetYaw = 0.0f;
	record.bTeleported = false;
}

// A snapshot whose pose matches the record above, so the only thing under test is
// the age comparison. flPoseReference is a CLIENT-clock pose time (curtime - lerp),
// the same basis LagRecord_t::SimulationTime is captured on - that identity is the
// contract these tests exist to pin. MaxBacktrackTime defaults to the window
// UpdateRecords would hand out on a stable connection.
LagRecordCachedState_t MakeCached(float flPoseReference,
	float flMaxBacktrackTime = LAG_MAX_BACKTRACK_TIME - LAG_BACKTRACK_SAFETY_MARGIN)
{
	LagRecordCachedState_t cached{};
	cached.AbsOrigin = Vec3(0.0f, 0.0f, 0.0f);
	cached.EyeAngles = Vec3(0.0f, 0.0f, 0.0f);
	cached.Flags = 0;
	cached.FeetYaw = 0.0f;
	cached.PoseReferenceTime = flPoseReference;
	cached.MaxBacktrackTime = flMaxBacktrackTime;
	return cached;
}

constexpr const char* kHitscanSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp";
constexpr const char* kAimbotSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/Aimbot.cpp";
constexpr const char* kAutoShootSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/AutoShoot/AutoShoot.cpp";
constexpr const char* kMeleeSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Aimbot/AimbotMelee/AimbotMelee.cpp";
constexpr const char* kBackstabSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Triggerbot/AutoBackstab/AutoBackstab.cpp";
constexpr const char* kMaterialsSource = "SEOwnedDE/SEOwnedDE/src/App/Features/Materials/Materials.cpp";

}  // namespace

TEST(LagRecordsBacktrackWindow, RecordAgeIsZeroWithoutAReferencePoint) {
    LagRecord_t record{};
    MakeRecord(record, 9.85f);

    // No record at all.
    EXPECT_FLOAT_EQ(CLagRecords::GetRecordAge(nullptr, MakeCached(10.0f)), 0.0f);

    // Unpopulated snapshot: PoseReferenceTime is seeded to -1. The age reads 0
    // rather than "ancient"; rejection of such a snapshot is MaxBacktrackTime's
    // job (see UnpopulatedSnapshotRejectsEveryRecord), not this function's.
    EXPECT_FLOAT_EQ(CLagRecords::GetRecordAge(&record, MakeCached(-1.0f)), 0.0f);

    // Unpopulated record.
    LagRecord_t unset{};
    EXPECT_FLOAT_EQ(CLagRecords::GetRecordAge(&unset, MakeCached(10.0f)), 0.0f);
}

TEST(LagRecordsBacktrackWindow, RecordAgeMeasuresElapsedClientTime) {
    LagRecord_t record{};
    MakeRecord(record, 9.85f);

    // Both terms are client-clock pose times, so the difference is the elapsed
    // client time since capture - which is exactly the rewind the shot requests
    // and exactly what the server's deviation tolerance bounds. This used to
    // subtract a client render time from the target's server-authored
    // m_flSimulationTime, which is not an elapsed time in any clock.
    EXPECT_NEAR(CLagRecords::GetRecordAge(&record, MakeCached(10.0f)), 0.15f, 1e-5f);
}

TEST(LagRecordsBacktrackWindow, RecordAgeClampsRecordsNewerThanTheSnapshot) {
    LagRecord_t record{};
    MakeRecord(record, 10.05f);

    EXPECT_FLOAT_EQ(CLagRecords::GetRecordAge(&record, MakeCached(10.0f)), 0.0f);
}

TEST(LagRecordsBacktrackWindow, AcceptsRecordsTheServerWillHonour) {
    LagRecord_t record{};

    MakeRecord(record, 10.0f - 0.01f);
    EXPECT_TRUE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f)));

    MakeRecord(record, 10.0f - 0.15f);
    EXPECT_TRUE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f)));

    MakeRecord(record, 10.0f - 0.17f);
    EXPECT_TRUE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f)));
}

TEST(LagRecordsBacktrackWindow, HoldsBackMarginFromTheServerCutoff) {
    LagRecord_t record{};

    // Between the usable window and the server's hard cutoff. Formerly accepted,
    // because the gate compared against the raw 0.2 s constant. The server tests
    // ITS OWN latency estimate against the correction our tick implies, and the two
    // never agree exactly, so a record sitting a hair inside 0.2 s is a coin flip -
    // honoured on some shots, silently discarded on others. Rejecting it is the
    // point: an un-stamped shot still gets the server's own correction and lands
    // where the client rendered, whereas a discarded stamp lands nowhere useful.
    MakeRecord(record, 10.0f - 0.19f);
    EXPECT_LT(CLagRecords::GetRecordAge(&record, MakeCached(10.0f)), LAG_MAX_BACKTRACK_TIME);
    EXPECT_FALSE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f)));
}

TEST(LagRecordsBacktrackWindow, WindowComesFromTheSnapshotNotTheConstant) {
    LagRecord_t record{};
    MakeRecord(record, 10.0f - 0.12f);

    // UpdateRecords narrows the window as measured latency jitter grows, so the
    // per-record verdict has to follow the snapshot rather than a fixed constant.
    EXPECT_TRUE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f, 0.175f)));
    EXPECT_FALSE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f, 0.10f)));
}

TEST(LagRecordsBacktrackWindow, UnpopulatedSnapshotRejectsEveryRecord) {
    LagRecord_t record{};
    MakeRecord(record, 9.99f);

    // A default-constructed snapshot leaves MaxBacktrackTime at 0, so nothing is
    // reachable through it. Fail-closed on purpose: a zeroed snapshot used to make
    // GetRecordAge return 0 for every record, which read as "all fresh" and offered
    // the entire ring - including records hundreds of ms deep - to a shot.
    const LagRecordCachedState_t empty{};
    EXPECT_FLOAT_EQ(empty.MaxBacktrackTime, 0.0f);
    EXPECT_FALSE(CLagRecords::IsWithinBacktrackWindow(&record, empty));
}

TEST(LagRecordsBacktrackWindow, RejectsRecordsPastTheServerTolerance) {
    LagRecord_t record{};

    // Just past the tolerance. Deliberately not asserted exactly ON it: float
    // rounding of 10.0f - 0.2f lands a hair inside the window, and the point of
    // the gate is what it rejects, not where the last representable bit falls.
    MakeRecord(record, 10.0f - (LAG_MAX_BACKTRACK_TIME + 0.001f));
    EXPECT_FALSE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f)));

    // The reported symptom: a ~300 ms record was being handed to shots, the
    // server threw the tick away, and the bullet landed near the present.
    MakeRecord(record, 10.0f - 0.3f);
    EXPECT_FALSE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f)));

    // Deepest slot in the ring at 66 tick.
    MakeRecord(record, 10.0f - (MAX_LAG_RECORDS / 66.0f));
    EXPECT_FALSE(CLagRecords::IsWithinBacktrackWindow(&record, MakeCached(10.0f)));
}

TEST(LagRecordsBacktrackWindow, RingIsDeeperThanTheWindowSoTheGateHasWorkToDo) {
    // If the ring could not hold a record older than the window, the age gate
    // would be dead code and this whole file would be testing nothing. It can:
    // 24 slots is ~364 ms at 66 tick against a 200 ms window.
    EXPECT_GT(static_cast<float>(MAX_LAG_RECORDS) / 66.0f, LAG_MAX_BACKTRACK_TIME);
    EXPECT_FLOAT_EQ(LAG_MAX_BACKTRACK_TIME, 0.2f);
}

TEST(LagRecordsBacktrackWindow, MarginConstantsLeaveAUsableWindow) {
    // The margin has to buy real headroom without eating the whole window, and the
    // floor has to sit below the nominal window or a stable connection would be
    // clamped up to it. Amalgam ships a 185 ms default against the same 200 ms
    // server rule, so this is the same order of headroom, not a guess.
    EXPECT_GT(LAG_BACKTRACK_SAFETY_MARGIN, 0.0f);
    EXPECT_LT(LAG_BACKTRACK_SAFETY_MARGIN, LAG_MAX_BACKTRACK_TIME * 0.5f);
    EXPECT_GT(LAG_BACKTRACK_JITTER_SCALE, 1.0f);
    EXPECT_LT(LAG_MIN_BACKTRACK_TIME, LAG_MAX_BACKTRACK_TIME - LAG_BACKTRACK_SAFETY_MARGIN);
    EXPECT_GT(LAG_MIN_BACKTRACK_TIME, 0.0f);
}

TEST(LagRecordsTickOwnership, AutoShootDoesNotFabricateATickForTheLivePose) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kAutoShootSource);

    // AutoShoot traces the live interpolated pose and consults no records at all,
    // so the incoming tick is already correct for its shot. It used to stamp a
    // fabricated historical tick from the target's simulation time - a pose that
    // was never recorded, and the same defect already removed from hitscan, melee
    // and backstab. This call site was missed.
    //
    // Matched on the assignment rather than the macro name so the comment that
    // documents the removal cannot satisfy the assertion.
    EXPECT_EQ(src.find("pCmd->tick_count ="), std::string::npos);
    EXPECT_EQ(src.find("tick_count +="), std::string::npos);

    // And it must respect an upstream owner, the way AutoBackstab already does.
    EXPECT_NE(src.find("if (G::bCommandTickResolved)"), std::string::npos);
}

TEST(LagRecordsBacktrackWindow, UsabilityRejectsMissingRecordAndMissingOwner) {
    const auto cached = MakeCached(10.0f);

    EXPECT_FALSE(CLagRecords::IsRecordUsable(nullptr, cached));

    // Player is left null by MakeRecord, which is what a slot looks like before
    // its first commit; the owner check short-circuits before any netvar read.
    LagRecord_t record{};
    MakeRecord(record, 9.95f);
    EXPECT_FALSE(CLagRecords::IsRecordUsable(&record, cached));
}

TEST(LagRecordsPoseDifference, RejectsTheInterpolatedPresent) {
    // The newest record is captured from the same pose the cached snapshot is
    // built from, so it is bit-identical to the live state. Backtracking onto it
    // would rewind to where the target already is - the shot gains nothing and
    // the server may still refuse the tick. Minimum useful depth is record 1.
    LagRecord_t record{};
    MakeRecord(record, 10.0f);

    EXPECT_FALSE(CLagRecords::DiffersFromCurrentCached(&record, MakeCached(10.0f)));
}

TEST(LagRecordsPoseDifference, AcceptsMovementAboveTheThresholds) {
    auto cached = MakeCached(10.0f);
    LagRecord_t record{};

    MakeRecord(record, 9.9f);
    record.Flags = 1;
    EXPECT_TRUE(CLagRecords::DiffersFromCurrentCached(&record, cached));

    MakeRecord(record, 9.9f);
    record.FeetYaw = 0.6f;
    EXPECT_TRUE(CLagRecords::DiffersFromCurrentCached(&record, cached));

    MakeRecord(record, 9.9f);
    record.AbsOrigin = Vec3(0.6f, 0.0f, 0.0f);
    EXPECT_TRUE(CLagRecords::DiffersFromCurrentCached(&record, cached));

    MakeRecord(record, 9.9f);
    record.EyeAngles = Vec3(0.0f, 0.6f, 0.0f);
    EXPECT_TRUE(CLagRecords::DiffersFromCurrentCached(&record, cached));

    MakeRecord(record, 9.9f);
    record.EyeAngles = Vec3(0.6f, 0.0f, 0.0f);
    EXPECT_TRUE(CLagRecords::DiffersFromCurrentCached(&record, cached));
}

TEST(LagRecordsPoseDifference, IgnoresSubThresholdJitter) {
    auto cached = MakeCached(10.0f);
    LagRecord_t record{};

    MakeRecord(record, 9.9f);
    record.FeetYaw = 0.4f;
    record.AbsOrigin = Vec3(0.4f, 0.0f, 0.0f);
    record.EyeAngles = Vec3(0.4f, 0.4f, 0.0f);

    EXPECT_FALSE(CLagRecords::DiffersFromCurrentCached(&record, cached));
}

TEST(LagRecordsPoseDifference, HandlesYawWrapWithoutFalsePositives) {
    LagRecord_t record{};
    MakeRecord(record, 9.9f);

    // 179.9 vs -179.9 is 0.2 degrees apart, not 359.8. A naive subtraction here
    // would call a stationary target "moved" on every single record.
    auto cached = MakeCached(10.0f);
    cached.EyeAngles = Vec3(0.0f, 179.9f, 0.0f);
    record.EyeAngles = Vec3(0.0f, -179.9f, 0.0f);
    EXPECT_FALSE(CLagRecords::DiffersFromCurrentCached(&record, cached));

    // 179.0 vs -179.0 really is 2 degrees apart.
    cached.EyeAngles = Vec3(0.0f, 179.0f, 0.0f);
    record.EyeAngles = Vec3(0.0f, -179.0f, 0.0f);
    EXPECT_TRUE(CLagRecords::DiffersFromCurrentCached(&record, cached));
}

TEST(LagRecordsCachedState, GuardsOutOfRangeEntityIndices) {
    // A recycled or hostile entindex must yield an empty snapshot rather than an
    // out-of-bounds read. Empty means PoseReferenceTime == -1 and
    // MaxBacktrackTime == 0, so no record resolves through it.
    EXPECT_FLOAT_EQ(F::LagRecords->GetCachedState(0).PoseReferenceTime, -1.0f);
    EXPECT_FLOAT_EQ(F::LagRecords->GetCachedState(-1).PoseReferenceTime, -1.0f);
    EXPECT_FLOAT_EQ(F::LagRecords->GetCachedState(MAX_PLAYERS).PoseReferenceTime, -1.0f);
    EXPECT_FLOAT_EQ(F::LagRecords->GetCachedState(MAX_PLAYERS + 1024).PoseReferenceTime, -1.0f);
    EXPECT_FLOAT_EQ(F::LagRecords->GetCachedState(0).MaxBacktrackTime, 0.0f);
}

// The rest of this file asserts on source text. These paths all need a live
// CUserCmd, a live C_TFPlayer and the engine's own view angles to exercise, so
// there is nothing to execute headless - but the invariant they encode is the one
// that made manual shots miss, and it must not silently regress.

TEST(LagRecordsTickOwnership, HitscanOnlyRewindsShotsTheAimbotActuallyAimed) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kHitscanSource);

    // The command's angles are compared against the target the tick came from,
    // instead of assuming Aim() ran. Aimbot_Key is unbound by default, so on a
    // stock config the aimbot never moved the view yet still rewrote the tick.
    EXPECT_NE(src.find("bAimbotDirectedShot"), std::string::npos);
    EXPECT_NE(src.find("vAimError"), std::string::npos);

    // A live-pose winner has no record to rewind to, but still claims the command:
    // keeping the incoming tick is a decision, and leaving the flag clear let the
    // triggerbot and the manual resolver retarget an already-aimed shot.
    EXPECT_NE(src.find("if (target.LagRecord)"), std::string::npos);
    EXPECT_NE(src.find("G::bCommandTickResolved = true;"), std::string::npos);

    // The manual resolver must NOT be called from inside this feature any more.
    // Every early return in Run and in CAimbot::RunMain above it used to swallow
    // the user's backtrack - Aimbot_Active off, cursor visible, cloaked, taunting,
    // Auto Scope, a building winning the FOV sort, the minigun spin-up hack. It is
    // driven once from CAimbot::Run instead, where nothing has returned yet.
    EXPECT_EQ(src.find("ResolveManualShot(pCmd, pLocal);"), std::string::npos);
}

TEST(LagRecordsTickOwnership, ManualResolverIsDrivenOutsideTheAimbotGate) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kAimbotSource);

    // Driven from CAimbot::Run, after RunMain returns, so no early return inside
    // RunMain can drop it.
    EXPECT_NE(src.find("F::AimbotHitscan->ResolveManualShot(pCmd, pLocalManual)"), std::string::npos);

    // Gated on the flag latched BEFORE RunMain. Re-deriving "am I firing" here
    // would read false on a spinning minigun, because the spin-up hack clears
    // G::bCanPrimaryAttack during RunMain.
    EXPECT_NE(src.find("G::bManualHitscanFiring && !G::bCommandTickResolved"), std::string::npos);

    const auto runMainPos = src.find("RunMain(pCmd);");
    const auto resolvePos = src.find("F::AimbotHitscan->ResolveManualShot");
    ASSERT_NE(runMainPos, std::string::npos);
    ASSERT_NE(resolvePos, std::string::npos);
    EXPECT_LT(runMainPos, resolvePos);
}

TEST(LagRecordsTickOwnership, MeleeVerifiesAimAndInstalledPose) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMeleeSource);

    EXPECT_NE(src.find("bAimbotDirectedSwing"), std::string::npos);
    EXPECT_NE(src.find("if (bAimbotDirectedSwing)"), std::string::npos);
    EXPECT_NE(src.find("CLagRecords::GetCommandTick(target.SimulationTime)"), std::string::npos);

    // A scope that failed to install leaves the live pose in place, so the trace
    // would validate the present while the stamp commits a historical tick.
    EXPECT_NE(src.find("target.LagRecord && !scope.IsActive()"), std::string::npos);
}

TEST(LagRecordsTickOwnership, BackstabNeverFabricatesATickForTheLivePose) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kBackstabSource);

    // Bails when an earlier feature already resolved this command's shot.
    EXPECT_NE(src.find("if (G::bCommandTickResolved)"), std::string::npos);

    // The historical branch uses the shared pose/tick pairing and claims the tick.
    EXPECT_NE(src.find("CLagRecords::GetCommandTick(record->SimulationTime)"), std::string::npos);
    EXPECT_GE(testhelpers::CountOccurrences(src, "G::bCommandTickResolved = true;"), 2u);

    // The live branch used to stamp m_flSimulationTime + lerp for a pose that was
    // never recorded. No hand-rolled tick arithmetic is left in this file.
    EXPECT_EQ(src.find("TIME_TO_TICKS"), std::string::npos);

    EXPECT_NE(src.find("if (!scope.IsActive())"), std::string::npos);
}

TEST(LagRecordsGhost, ShowsTheRecordAShotWouldPick) {
    const auto root = testhelpers::FindRepoRoot();
    const auto src = testhelpers::ReadTextFile(root / kMaterialsSource);

    // "Last Only" (the default style) draws the newest USABLE record - the same
    // one every consumer selects - instead of the oldest slot in the ring.
    EXPECT_NE(src.find("CLagRecords::IsRecordUsable(pCandidate, cachedState)"), std::string::npos);
    EXPECT_NE(src.find("pRecord = pCandidate;"), std::string::npos);

    // Both draw sites skip a record whose bones failed to install, so a ghost is
    // never a full-opacity copy of the present.
    EXPECT_GE(testhelpers::CountOccurrences(src, "if (!scope.IsActive())"), 2u);
}
