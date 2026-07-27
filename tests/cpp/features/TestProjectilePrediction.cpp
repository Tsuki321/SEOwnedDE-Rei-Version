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

TEST(ProjectileTiming, DefaultTolerance_IsHalfTick) {
	EXPECT_NEAR(ProjectilePredictionMath::ResolveTemporalTolerance(0.015f), 0.0075f, 1e-6f);
	EXPECT_NEAR(ProjectilePredictionMath::ResolveTemporalTolerance(0.015f, 0.02f), 0.0075f, 1e-6f);
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

TEST(QuarticSolver, StationaryTarget_Gravity_SelectsDistinctLowAndHighArcs) {
	BallisticSolver::SolverParams lowParams;
	lowParams.ShootPos = Vec3(0, 0, 0);
	lowParams.TargetPos = Vec3(100, 0, 0);
	lowParams.Speed = 1000.0f;
	lowParams.Gravity = 800.0f;

	BallisticSolver::SolverParams highParams = lowParams;
	highParams.UseHighArc = true;

	const BallisticSolver::SolveResult low = BallisticSolver::SolveBallistic(lowParams);
	const BallisticSolver::SolveResult high = BallisticSolver::SolveBallistic(highParams);

	ASSERT_TRUE(low.Valid);
	ASSERT_TRUE(high.Valid);
	EXPECT_GE(low.PositiveRootCount, 2);
	EXPECT_GE(high.PositiveRootCount, 2);
	EXPECT_LT(low.Time, 0.2f);
	EXPECT_GT(high.Time, 2.0f);
	EXPECT_GT(high.Time, low.Time + 1.0f);

	Vec3 lowHit = lowParams.ShootPos + low.Direction * lowParams.Speed * low.Time;
	lowHit.z -= 0.5f * lowParams.Gravity * low.Time * low.Time;
	Vec3 highHit = highParams.ShootPos + high.Direction * highParams.Speed * high.Time;
	highHit.z -= 0.5f * highParams.Gravity * high.Time * high.Time;
	EXPECT_LT(lowHit.DistTo(lowParams.TargetPos), 0.05f);
	EXPECT_LT(highHit.DistTo(highParams.TargetPos), 0.05f);
	EXPECT_LT(low.EndpointError, 0.05f);
	EXPECT_LT(high.EndpointError, 0.05f);
}

TEST(QuarticSolver, HighArcOutsideMaxTime_DoesNotFallBackToLowArc) {
	BallisticSolver::SolverParams p;
	p.TargetPos = Vec3(100, 0, 0);
	p.Speed = 1000.0f;
	p.Gravity = 800.0f;
	p.UseHighArc = true;
	p.MaxTime = 1.0f;

	const BallisticSolver::SolveResult result = BallisticSolver::SolveBallistic(p);
	EXPECT_FALSE(result.Valid);
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
	EXPECT_TRUE(rWithDrag.DragConverged);

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
	EXPECT_TRUE(r.DragConverged);

	const float dragDistance = p.Speed * (1.0f - std::exp(-p.DragCoeff * r.Time)) / p.DragCoeff;
	Vec3 hitPos = p.ShootPos + r.Direction * dragDistance;
	hitPos.z -= 0.5f * p.Gravity * r.Time * r.Time;
	EXPECT_LT(hitPos.DistTo(p.TargetPos), 0.25f);
	EXPECT_LT(r.EndpointError, 0.05f);
}

TEST(QuarticSolver, WithDrag_ExhaustedIterationBudgetIsInvalid) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(800, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.DragCoeff = BallisticSolver::ComputeDragCoefficient(BallisticSolver::WeaponClass::GrenadeLauncher);
	p.DragIters = 0;

	const BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	EXPECT_FALSE(r.Valid);
	EXPECT_FALSE(r.DragConverged);
	EXPECT_GT(r.Time, 0.0f);
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
		pos.x += 3.0f;
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
		pos.x += 3.0f;
	}

	int startTick = BallisticSolver::BinarySearchMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);

	ASSERT_GE(startTick, 0);

	BallisticSolver::NewtonRefineResult r = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, startTick, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 3);

	EXPECT_TRUE(r.Valid);
	EXPECT_GE(r.Tick, 0);
	EXPECT_LT(r.Tick, static_cast<int>(path.size()));
	EXPECT_LE(r.TemporalResidual, 0.5f * 0.015f);
}

TEST(NewtonRefine, ShortPathRejectsHorizonLimitedIntercept) {
	std::vector<Vec3> path(20, Vec3(1000.0f, 0.0f, 0.0f));

	const BallisticSolver::NewtonRefineResult r = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, 10, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 3);

	EXPECT_FALSE(r.Valid);
	EXPECT_TRUE(r.HorizonLimited);
	EXPECT_TRUE(r.AtPathBoundary);
	EXPECT_GT(r.RequiredTime, r.PathHorizon);
	EXPECT_GT(r.TemporalResidual, 0.5f * 0.015f);
}

TEST(NewtonRefine, SubTickInterpolationConvergesFromPoorInitialGuess) {
	std::vector<Vec3> path;
	Vec3 position(800.0f, 0.0f, 0.0f);
	for (int i = 0; i < 100; ++i)
	{
		path.push_back(position);
		position.x += 5.0f;
	}

	const BallisticSolver::NewtonRefineResult r = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, 0, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 1);

	ASSERT_TRUE(r.Valid);
	EXPECT_LT(r.TemporalResidual, 1e-4f);
	EXPECT_NEAR(r.RequiredTime, r.SimulatedTime, 1e-4f);
	EXPECT_GT(r.SimulatedTime, 1.0f);
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

TEST(QuarticRoots, BracketIsolationReturnsOrderedPositiveRoots) {
	const BallisticSolver::QuarticRoots roots = BallisticSolver::FindPositiveQuarticRoots(
		1.0, 0.0, -5.0, 0.0, 4.0);

	ASSERT_EQ(roots.Count, 2);
	EXPECT_NEAR(roots.Values[0], 1.0, 1e-7);
	EXPECT_NEAR(roots.Values[1], 2.0, 1e-7);
}

// =============================================================================
// Scan Meeting Tick (full-path scan, robust to non-monotonic residuals)
// =============================================================================

TEST(ScanMeetingTick, FindsCorrectTick_StationaryTarget) {
	std::vector<Vec3> path;
	for (int i = 0; i < 100; i++)
		path.push_back(Vec3(1000.0f, 0.0f, 0.0f));

	// travel time is exactly 1.0s -> 1.0 / 0.015 = 66.67, best tick is 67
	int tick = BallisticSolver::ScanMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);

	EXPECT_GE(tick, 65);
	EXPECT_LE(tick, 68);
}

TEST(ScanMeetingTick, TimingBiasShiftsTickOutward) {
	std::vector<Vec3> path;
	for (int i = 0; i < 100; i++)
		path.push_back(Vec3(1000.0f, 0.0f, 0.0f));

	int unbiased = BallisticSolver::ScanMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false, 0.0f);

	// 0.15s bias = 10 ticks, expect the meeting tick to move out by roughly that
	int biased = BallisticSolver::ScanMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false, 0.15f);

	EXPECT_GE(unbiased, 0);
	EXPECT_GE(biased, unbiased + 8);
}

TEST(ScanMeetingTick, DragUsesConvergedFlightTime) {
	std::vector<Vec3> path(200, Vec3(1500.0f, 0.0f, 0.0f));
	const float dragCoeff = BallisticSolver::ComputeDragCoefficient(
		BallisticSolver::WeaponClass::Cannonball);

	BallisticSolver::SolverParams params;
	params.TargetPos = path.front();
	params.Speed = 1200.0f;
	params.DragCoeff = dragCoeff;
	params.DragIters = 3;
	const BallisticSolver::SolveResult solve = BallisticSolver::SolveBallistic(params);
	ASSERT_TRUE(solve.Valid);
	ASSERT_TRUE(solve.DragConverged);

	const int tick = BallisticSolver::ScanMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1200.0f, 0.0f, 0.0f,
		dragCoeff, 2.5f, false);
	const int expectedTick = static_cast<int>(std::round(solve.Time / 0.015f));

	EXPECT_NEAR(tick, expectedTick, 1);
	EXPECT_GE(tick, 85);
}

TEST(ScanMeetingTick, NonMonotonicPath_StillFinds) {
	std::vector<Vec3> path;
	Vec3 pos(800.0f, 0.0f, 0.0f);
	for (int i = 0; i < 100; i++)
	{
		path.push_back(pos);
		pos.x += 5.0f;
		pos.z = 50.0f * std::sin((i + 1) * 0.06); // jump-like height oscillation
	}

	int tick = BallisticSolver::ScanMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);

	EXPECT_GE(tick, 0);
	EXPECT_LT(tick, static_cast<int>(path.size()));
}

TEST(ScanMeetingTick, EmptyPath_ReturnsNegativeOne) {
	std::vector<Vec3> path;
	int tick = BallisticSolver::ScanMeetingTick(
		path, 0.015f, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, 1.5f, false);
	EXPECT_EQ(tick, -1);
}

// =============================================================================
// View-Up Muzzle Compensation (opt-in)
// =============================================================================

TEST(ViewUpMuzzle, DefaultOff_MatchesLegacyBehavior) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.MuzzleUpZ = 200.0f;
	p.DragCoeff = 0.0f;
	// UseViewUpMuzzle left at default (false)

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_GT(r.Direction.z, 0.0f);
}

TEST(ViewUpMuzzle, Enabled_ProducesValidUnitDirection) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.MuzzleUpZ = 200.0f;
	p.UseViewUpMuzzle = true;
	p.DragCoeff = 0.0f;

	BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);

	ASSERT_TRUE(r.Valid);
	EXPECT_GT(r.Time, 0.0f);
	EXPECT_NEAR(r.Direction.LengthSqr(), 1.0f, 2e-3f);
}

TEST(ViewUpMuzzle, Enabled_ReconstructsVerticalImpulseEndpoint) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(10, -20, 30);
	p.TargetPos = Vec3(1000, 250, 80);
	p.TargetVel = Vec3(75, -20, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.MuzzleUpZ = 200.0f;
	p.UseViewUpMuzzle = true;

	const BallisticSolver::SolveResult r = BallisticSolver::SolveBallistic(p);
	ASSERT_TRUE(r.Valid);

	const float horizontalLength = std::sqrt(
		r.Direction.x * r.Direction.x + r.Direction.y * r.Direction.y);
	ASSERT_GT(horizontalLength, 1e-4f);
	const Vec3 viewUp(
		-r.Direction.z * r.Direction.x / horizontalLength,
		-r.Direction.z * r.Direction.y / horizontalLength,
		horizontalLength);

	Vec3 projectileAtImpact = p.ShootPos + r.Direction * p.Speed * r.Time + viewUp * p.MuzzleUpZ * r.Time;
	projectileAtImpact.z -= 0.5f * p.Gravity * r.Time * r.Time;
	const Vec3 targetAtImpact = p.TargetPos + p.TargetVel * r.Time;
	EXPECT_LT(projectileAtImpact.DistTo(targetAtImpact), 0.1f);
}

TEST(ViewUpMuzzle, Enabled_DiffersSlightlyFromLegacy) {
	BallisticSolver::SolverParams p;
	p.ShootPos = Vec3(0, 0, 0);
	p.TargetPos = Vec3(1000, 0, 0);
	p.TargetVel = Vec3(0, 0, 0);
	p.Speed = 1200.0f;
	p.Gravity = 800.0f;
	p.MuzzleUpZ = 200.0f;
	p.DragCoeff = 0.0f;

	BallisticSolver::SolverParams pViewUp = p;
	pViewUp.UseViewUpMuzzle = true;

	BallisticSolver::SolveResult rLegacy = BallisticSolver::SolveBallistic(p);
	BallisticSolver::SolveResult rViewUp = BallisticSolver::SolveBallistic(pViewUp);

	ASSERT_TRUE(rLegacy.Valid);
	ASSERT_TRUE(rViewUp.Valid);

	// refinement should only be a small correction, not a wildly different arc
	EXPECT_NEAR(rLegacy.Time, rViewUp.Time, 0.1f);
}

// =============================================================================
// Newton Refinement - Timing Bias
// =============================================================================

TEST(NewtonRefine, TimingBiasShiftsTickOutward) {
	std::vector<Vec3> path;
	Vec3 pos(800.0f, 0.0f, 0.0f);
	for (int i = 0; i < 100; i++)
	{
		path.push_back(pos);
		pos.x += 5.0f;
	}

	BallisticSolver::NewtonRefineResult rUnbiased = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, 0, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 3, 0.0f);

	BallisticSolver::NewtonRefineResult rBiased = BallisticSolver::NewtonRefineOverPath(
		path, 0.015f, 0, Vec3(0, 0, 0), 1000.0f, 0.0f, 0.0f, 0.0f, false, 3, 0.15f);

	EXPECT_TRUE(rUnbiased.Valid);
	EXPECT_TRUE(rBiased.Valid);
	EXPECT_GE(rBiased.Tick, rUnbiased.Tick);
}
