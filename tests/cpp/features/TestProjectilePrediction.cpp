#include <gtest/gtest.h>

#include "App/Features/Aimbot/AimbotProjectile/AimbotProjectilePrediction.h"

#include <cmath>
#include <vector>
#include <limits>

// =============================================================================
// SIMD Helpers
// =============================================================================

TEST(SimdHelpers, FastSqrt_PerfectSquare) {
	EXPECT_NEAR(Simd::FastSqrt(16.0f), 4.0f, 1e-5f);
	EXPECT_NEAR(Simd::FastSqrt(100.0f), 10.0f, 1e-5f);
}

TEST(SimdHelpers, FastSqrt_Zero) {
	EXPECT_FLOAT_EQ(Simd::FastSqrt(0.0f), 0.0f);
}

TEST(SimdHelpers, FastRSqrt_ReciprocalOfSqrt) {
	EXPECT_NEAR(Simd::FastRSqrt(4.0f), 0.5f, 1e-5f);
	EXPECT_NEAR(Simd::FastRSqrt(100.0f), 0.1f, 1e-5f);
}

TEST(SimdHelpers, FastRSqrt_NormalizedMatchesLength) {
	Vec3 v(3.0f, 4.0f, 0.0f);
	Vec3 n = Simd::NormalizedFast(v);
	EXPECT_NEAR(n.x, 0.6f, 1e-4f);
	EXPECT_NEAR(n.y, 0.8f, 1e-4f);
	EXPECT_NEAR(n.z, 0.0f, 1e-4f);
}

TEST(SimdHelpers, NormalizedFast_ZeroVectorReturnsZero) {
	Vec3 v(0.0f, 0.0f, 0.0f);
	Vec3 n = Simd::NormalizedFast(v);
	EXPECT_FLOAT_EQ(n.x, 0.0f);
	EXPECT_FLOAT_EQ(n.y, 0.0f);
	EXPECT_FLOAT_EQ(n.z, 0.0f);
}

TEST(SimdHelpers, FastDot3_MatchesScalar) {
	float result = Simd::FastDot3(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f);
	EXPECT_NEAR(result, 32.0f, 1e-4f);
}

// =============================================================================
// Timing Helpers (existing, preserved)
// =============================================================================

TEST(ProjectileTiming, ComputeTimingBias_SumsBoth) {
	EXPECT_FLOAT_EQ(ProjectilePredictionMath::ComputeTimingBias(0.05f, 0.03f), 0.08f);
}

TEST(ProjectileTiming, ComputeTimingBias_ClampsNegative) {
	EXPECT_FLOAT_EQ(ProjectilePredictionMath::ComputeTimingBias(-0.1f, 0.0f), 0.0f);
}

TEST(ProjectileTiming, ComputeTemporalResidual_AbsoluteDifference) {
	EXPECT_NEAR(ProjectilePredictionMath::ComputeTemporalResidual(1.0f, 0.8f, 0.1f), 0.1f, 1e-5f);
}

TEST(ProjectileTiming, IsWithinTemporalTolerance_Within) {
	EXPECT_TRUE(ProjectilePredictionMath::IsWithinTemporalTolerance(0.02f, 0.05f));
}

TEST(ProjectileTiming, IsWithinTemporalTolerance_Exceeds) {
	EXPECT_FALSE(ProjectilePredictionMath::IsWithinTemporalTolerance(0.06f, 0.05f));
}

// =============================================================================
// Drag Coefficient from vPhysics Basis
// =============================================================================

TEST(DragBasis, GrenadeLauncher_HasNonZeroDrag) {
	float k = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::GrenadeLauncher);
	EXPECT_GT(k, 0.0f);
	EXPECT_NEAR(k, 0.007942f * 2.0f, 1e-4f);
}

TEST(DragBasis, Cannonball_HasHigherDragThanGrenade) {
	float kPipe = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::GrenadeLauncher);
	float kCannon = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::Cannonball);
	EXPECT_GT(kCannon, kPipe);
}

TEST(DragBasis, None_HasZeroDrag) {
	float k = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::None);
	EXPECT_FLOAT_EQ(k, 0.0f);
}

TEST(DragBasis, Stickybomb_DiffersFromGrenade) {
	float kPipe = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::GrenadeLauncher);
	float kSticky = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::Stickybomb);
	EXPECT_NE(kPipe, kSticky);
	EXPECT_GT(kSticky, 0.0f);
}

// =============================================================================
// Exponential Drag Effective Speed
// =============================================================================

TEST(ExponentialDrag, ZeroDrag_ReturnsOriginalSpeed) {
	EXPECT_FLOAT_EQ(BallisticSolver::ExponentialDragEffectiveSpeed(1200.0f, 0.0f, 1.0f), 1200.0f);
}

TEST(ExponentialDrag, ZeroTime_ReturnsOriginalSpeed) {
	EXPECT_FLOAT_EQ(BallisticSolver::ExponentialDragEffectiveSpeed(1200.0f, 0.01f, 0.0f), 1200.0f);
}

TEST(ExponentialDrag, ReducesSpeed) {
	float s = BallisticSolver::ExponentialDragEffectiveSpeed(1200.0f, 0.05f, 1.0f);
	EXPECT_LT(s, 1200.0f);
	EXPECT_GT(s, 1100.0f);
}

TEST(ExponentialDrag, SmallDrag_ApproximatesLinearDecay) {
	float k = 0.001f;
	float t = 1.0f;
	float speed = 1000.0f;
	float expected = speed * (1.0f - k * t / 2.0f);
	float actual = BallisticSolver::ExponentialDragEffectiveSpeed(speed, k, t);
	EXPECT_NEAR(actual, expected, 1e-3f);
}

// =============================================================================
// Quartic Solver - Stationary Target (No Gravity)
// =============================================================================

TEST(QuarticSolver, StationaryTarget_NoGravity_DirectHit) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 1000.0f;
	p.Gravity = 0.0f;
	p.MuzzleUpZ = 0.0f;
	p.DragCoeff = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Time, 1.0f, 1e-3f);
	EXPECT_NEAR(r.Direction.x, 1.0f, 1e-3f);
	EXPECT_NEAR(r.Direction.y, 0.0f, 1e-3f);
	EXPECT_NEAR(r.Direction.z, 0.0f, 1e-3f);
}

TEST(QuarticSolver, StationaryTarget_NoGravity_2DHit) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(300, 400, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 1000.0f;
	p.Gravity = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Time, 0.5f, 1e-3f);
	EXPECT_NEAR(r.Direction.x, 0.6f, 1e-3f);
	EXPECT_NEAR(r.Direction.y, 0.8f, 1e-3f);
}

TEST(QuarticSolver, StationaryTarget_NoGravity_Unreachable) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 500.0f;
	p.Gravity = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Time, 2.0f, 1e-2f);
}

// =============================================================================
// Quartic Solver - Stationary Target (With Gravity)
// =============================================================================

TEST(QuarticSolver, StationaryTarget_Gravity_LowArc) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 1000.0f;
	p.Gravity = 800.0f;
	p.MuzzleUpZ = 0.0f;
	p.UseHighArc = false;
	p.DragCoeff = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Time, std::sqrt(1.25f), 1e-2f);
	EXPECT_GT(r.Direction.x, 0.0f);
	EXPECT_GT(r.Direction.z, 0.0f);

	float dirLen = std::sqrt(r.Direction.x * r.Direction.x + r.Direction.y * r.Direction.y + r.Direction.z * r.Direction.z);
	EXPECT_NEAR(dirLen, 1.0f, 1e-3f);
}

TEST(QuarticSolver, StationaryTarget_Gravity_DirectionIsUnitVector) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(500, 300, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	float lenSq = r.Direction.x * r.Direction.x + r.Direction.y * r.Direction.y + r.Direction.z * r.Direction.z;
	EXPECT_NEAR(lenSq, 1.0f, 1e-3f);
}

TEST(QuarticSolver, StationaryTarget_Gravity_Unreachable_TooFar) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(10000, 0, 0);
	p.Speed = 100.0f;
	p.Gravity = 800.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	EXPECT_FALSE(r.Valid);
}

// =============================================================================
// Quartic Solver - Moving Target (No Gravity)
// =============================================================================

TEST(QuarticSolver, MovingTarget_NoGravity_HeadOn) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(200, 0, 0);
	p.Speed = 1000.0f;
	p.Gravity = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Time, 1.25f, 1e-2f);

	Vec3 hitPos = p.ShootPos + r.Direction * p.Speed * r.Time;
	EXPECT_NEAR(hitPos.x, 1250.0f, 5.0f);
}

TEST(QuarticSolver, MovingTarget_NoGravity_Crossing) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(500, 500, 0);
	p.TargetVel = Vec3(0, -300, 0);
	p.Speed = 1000.0f;
	p.Gravity = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_GT(r.Time, 0.0f);

	Vec3 hitPos = p.ShootPos + r.Direction * p.Speed * r.Time;
	Vec3 targetAtHit = p.TargetPos + p.TargetVel * r.Time;
	float dist = hitPos.DistTo(targetAtHit);
	EXPECT_LT(dist, 5.0f);
}

TEST(QuarticSolver, MovingTarget_NoGravity_TargetFasterThanProjectile_Unreachable) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(100, 0, 0);
	p.TargetVel = Vec3(2000, 0, 0);
	p.Speed = 1000.0f;
	p.Gravity = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);
	EXPECT_FALSE(r.Valid);
}

// =============================================================================
// Quartic Solver - Moving Target (With Gravity)
// =============================================================================

TEST(QuarticSolver, MovingTarget_Gravity_Intercepts) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(800, 0, 0);
	p.TargetVel = Vec3(100, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_GT(r.Time, 0.0f);
	EXPECT_LT(r.Time, 2.0f);

	Vec3 hitPos = p.ShootPos + r.Direction * p.Speed * r.Time;
	hitPos.z -= 0.5f * p.Gravity * r.Time * r.Time;
	Vec3 targetAtHit = p.TargetPos + p.TargetVel * r.Time;
	float dist = hitPos.DistTo(targetAtHit);
	EXPECT_LT(dist, 10.0f);
}

// =============================================================================
// Quartic Solver - Pipe Muzzle-Up (+200z)
// =============================================================================

TEST(QuarticSolver, PipeMuzzleUp_ProducesValidSolution) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.MuzzleUpZ = 200.0f;
	p.DragCoeff = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_GT(r.Time, 0.0f);

	float lenSq = r.Direction.LengthSqr();
	EXPECT_NEAR(lenSq, 1.0f, 1e-3f);
}

TEST(QuarticSolver, PipeMuzzleUp_RequiresLessAimElevation) {
	BallisticSolver::SolverParams withMuzzle;
	withMuzzle.ShootPos = Vec3(0, 0, 0);
	withMuzzle.TargetPos = Vec3(1000, 0, 0);
	withMuzzle.Speed = 1200.0f;
	withMuzzle.Gravity = 800.0f;
	withMuzzle.MuzzleUpZ = 200.0f;

	BallisticSolver::SolverParams noMuzzle = withMuzzle;
	noMuzzle.MuzzleUpZ = 0.0f;

	BallisticSolver::SolveResult rWith = BallisticSolver::SolveBallistic(withMuzzle);
	BallisticSolver::SolveResult rWithout = BallisticSolver::SolveBallistic(noMuzzle);

	ASSERT_TRUE(rWith.Valid);
	ASSERT_TRUE(rWithout.Valid);

	EXPECT_LT(rWith.Direction.z, rWithout.Direction.z);
}

// =============================================================================
// Quartic Solver - Drag Correction
// =============================================================================

TEST(QuarticSolver, WithDrag_LongerTimeThanNoDrag) {
	BallisticSolver::SolverParams noDrag;
	noDrag.ShootPos = Vec3(0, 0, 0);
	noDrag.TargetPos = Vec3(1500, 0, 0);
	noDrag.Speed = 1200.0f;
	noDrag.Gravity = 800.0f;
	noDrag.DragCoeff = 0.0f;
	noDrag.DragIters = 3;

	BallisticSolver::SolverParams withDrag = noDrag;
	withDrag.DragCoeff = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::Cannonball);

	BallisticSolver::SolveResult rNoDrag = BallisticSolver::SolveBallistic(noDrag);
	BallisticSolver::SolveResult rWithDrag = BallisticSolver::SolveBallistic(withDrag);

	ASSERT_TRUE(rNoDrag.Valid);
	ASSERT_TRUE(rWithDrag.Valid);

	EXPECT_GT(rWithDrag.Time, rNoDrag.Time);
}

TEST(QuarticSolver, WithDrag_StillReachesTarget) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(800, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.DragCoeff = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::GrenadeLauncher);
	p.DragIters = 3;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_GT(r.Time, 0.0f);

	float lenSq = r.Direction.LengthSqr();
	EXPECT_NEAR(lenSq, 1.0f, 1e-2f);
}

// =============================================================================
// Binary Search Meeting Tick
// =============================================================================

TEST(BinarySearchMeetingTick, FindsCorrectTick_StationaryTarget) {
	std::vector<Vec3> path;
	for (int i = 0; i < 100; i++)
		path.push_back(Vec3(1000.0f, 0.0f, 0.0f));

	int tick = BallisticSolver::BinarySearchMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);

	EXPECT_GE(tick, 0);
	EXPECT_LT(tick, 100);
}

TEST(BinarySearchMeetingTick, FindsCorrectTick_LinearMotion) {
	std::vector<Vec3> path;
	Vec3 pos(1000, 0, 0);
	for (int i = 0; i < 100; i++)
	{
		path.push_back(pos);
		pos.x += 15.0f;
	}

	int tick = BallisticSolver::BinarySearchMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);

	EXPECT_GE(tick, 0);
	EXPECT_LT(tick, 100);
}

TEST(BinarySearchMeetingTick, EmptyPath_ReturnsNegativeOne) {
	std::vector<Vec3> path;
	int tick = BallisticSolver::BinarySearchMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);
	EXPECT_EQ(tick, -1);
}

// =============================================================================
// Newton-Raphson Refinement Over Path
// =============================================================================

TEST(NewtonRefine, ConvergesOnStationaryPath) {
	std::vector<Vec3> path;
	for (int i = 0; i < 100; i++)
		path.push_back(Vec3(1000.0f, 0.0f, 0.0f));

	BallisticSolver::NewtonRefineResult r = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, 50, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 3);

	EXPECT_TRUE(r.Valid);
	EXPECT_GT(r.Time, 0.0f);
	EXPECT_GE(r.Tick, 0);
	EXPECT_LT(r.Tick, static_cast<int>(path.size()));
}

TEST(NewtonRefine, ConvergesOnMovingPath) {
	std::vector<Vec3> path;
	Vec3 pos(800, 0, 0);
	for (int i = 0; i < 100; i++)
	{
		path.push_back(pos);
		pos.x += 15.0f;
	}

	int startTick = BallisticSolver::BinarySearchMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);

	ASSERT_GE(startTick, 0);

	BallisticSolver::NewtonRefineResult r = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, startTick, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 3);

	EXPECT_TRUE(r.Valid);
	EXPECT_GE(r.Tick, 0);
	EXPECT_LT(r.Tick, static_cast<int>(path.size()));
}

TEST(NewtonRefine, InvalidStartTick_ReturnsInvalid) {
	std::vector<Vec3> path;
	path.push_back(Vec3(1000, 0, 0));

	BallisticSolver::NewtonRefineResult r = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, -1, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 3);

	EXPECT_FALSE(r.Valid);
	EXPECT_EQ(r.Tick, -1);
}

// =============================================================================
// Direction to Angles
// =============================================================================

TEST(DirectionToAngles, ForwardX) {
	Vec3 angles = BallisticSolver::DirectionToAngles(Vec3(1, 0, 0));
	EXPECT_NEAR(angles.x, 0.0f, 1e-3f);
	EXPECT_NEAR(angles.y, 0.0f, 1e-3f);
}

TEST(DirectionToAngles, ForwardY) {
	Vec3 angles = BallisticSolver::DirectionToAngles(Vec3(0, 1, 0));
	EXPECT_NEAR(angles.y, 90.0f, 1e-3f);
}

// =============================================================================
// Quartic Solver - Direction is Always Unit Vector
// =============================================================================

TEST(QuarticSolver, DirectionIsUnitVector_NoGravity) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(300, 400, 0);
	p.TargetVel = Vec3(50, -30, 0);
	p.Speed = 1000.0f;
	p.Gravity = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);
	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Direction.LengthSqr(), 1.0f, 1e-3f);
}

TEST(QuarticSolver, DirectionIsUnitVector_WithGravity) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(600, 200, 0);
	p.TargetVel = Vec3(100, -50, 0);
	p.Speed = 1500.0f;
	p.Gravity = 800.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);
	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Direction.LengthSqr(), 1.0f, 1e-3f);
}

TEST(QuarticSolver, DirectionIsUnitVector_WithMuzzleUp) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(800, 100, 0);
	p.TargetVel = Vec3(80, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.MuzzleUpZ = 200.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);
	ASSERT_TRUE(r.Valid);
	EXPECT_NEAR(r.Direction.LengthSqr(), 1.0f, 1e-3f);
}

// =============================================================================
// Drag Consistency: Solver and vPhysics Basis Agree
// =============================================================================

TEST(DragConsistency, GrenadeLauncher_DragCoeffMatchesBasis) {
	float k = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::GrenadeLauncher);
	BallisticSolver::DragBasis basis = BallisticSolver::GetDragBasis(BallisticSolver::WeaponClass::GrenadeLauncher);

	float expectedK = basis.Linear * basis.AirDensity;
	EXPECT_NEAR(k, expectedK, 1e-6f);
}

TEST(DragConsistency, Cannonball_DragCoeffMatchesBasis) {
	float k = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::Cannonball);
	BallisticSolver::DragBasis basis = BallisticSolver::GetDragBasis(BallisticSolver::WeaponClass::Cannonball);

	float expectedK = basis.Linear * basis.AirDensity;
	EXPECT_NEAR(k, expectedK, 1e-6f);
}

TEST(DragConsistency, AllWeaponClasses_HaveConsistentBasis) {
	for (int i = 0; i <= static_cast<int>(BallisticSolver::WeaponClass::None); i++)
	{
		auto cls = static_cast<BallisticSolver::WeaponClass>(i);
		BallisticSolver::DragBasis basis = BallisticSolver::GetDragBasis(cls);
		float k = BallisticSolver::ComputeDragCoefficient(cls);

		if (cls == BallisticSolver::WeaponClass::None)
		{
			EXPECT_FLOAT_EQ(k, 0.0f);
		}
		else
		{
			EXPECT_NEAR(k, basis.Linear * basis.AirDensity, 1e-6f);
			EXPECT_GT(k, 0.0f);
		}
	}
}

// =============================================================================
// SolveQuadratic Helper
// =============================================================================

TEST(SolveQuadratic, TwoPositiveRoots_ReturnsSmaller) {
	float t = BallisticSolver::SolveQuadratic(1.0f, -5.0f, 4.0f);
	EXPECT_NEAR(t, 1.0f, 1e-4f);
}

TEST(SolveQuadratic, OnePositiveRoot) {
	float t = BallisticSolver::SolveQuadratic(1.0f, 1.0f, -6.0f);
	EXPECT_NEAR(t, 2.0f, 1e-4f);
}

TEST(SolveQuadratic, NoRealRoots_ReturnsNegative) {
	float t = BallisticSolver::SolveQuadratic(1.0f, 0.0f, 1.0f);
	EXPECT_LT(t, 0.0f);
}

TEST(SolveQuadratic, LinearFallback) {
	float t = BallisticSolver::SolveQuadratic(0.0f, 2.0f, -4.0f);
	EXPECT_NEAR(t, 2.0f, 1e-4f);
}

// =============================================================================
// Quartic Evaluation Helpers
// =============================================================================

TEST(QuarticEval, EvaluateAtKnownPoint) {
	float val = BallisticSolver::EvaluateQuartic(1.0f, 0.0f, -5.0f, 0.0f, 4.0f, 1.0f);
	EXPECT_NEAR(val, 0.0f, 1e-4f);
}

TEST(QuarticEval, DerivativeAtKnownPoint) {
	float deriv = BallisticSolver::EvaluateQuarticDerivative(1.0f, 0.0f, -5.0f, 0.0f, 1.0f);
	EXPECT_NEAR(deriv, -6.0f, 1e-4f);
}

TEST(QuarticNewton, ConvergesToRoot) {
	float t = BallisticSolver::SolveQuarticNewton(1.0f, 0.0f, -5.0f, 0.0f, 4.0f, 0.5f);
	EXPECT_NEAR(t, 1.0f, 1e-4f);
}

TEST(QuarticNewton, FindsSecondPositiveRoot) {
	float t = BallisticSolver::SolveQuarticNewton(1.0f, 0.0f, -5.0f, 0.0f, 4.0f, 3.0f);
	EXPECT_NEAR(t, 2.0f, 1e-4f);
}
