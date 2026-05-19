#include <gtest/gtest.h>

#include "Utils/Math/Math.h"
#include "Utils/Vector/Vector.h"

#include <cmath>
#include <cfloat>
#include <array>

// -----------------------------------------------------------------------------
// Macros
// -----------------------------------------------------------------------------

TEST(MathMacros, Deg2RadZero) {
    EXPECT_FLOAT_EQ(DEG2RAD(0.0f), 0.0f);
}

TEST(MathMacros, Deg2Rad180) {
    EXPECT_NEAR(DEG2RAD(180.0f), PI, 1e-6f);
}

TEST(MathMacros, Deg2Rad360) {
    EXPECT_NEAR(DEG2RAD(360.0f), 2.0f * PI, 1e-6f);
}

TEST(MathMacros, Deg2RadNegative) {
    EXPECT_NEAR(DEG2RAD(-90.0f), -PI / 2.0f, 1e-6f);
}

TEST(MathMacros, Rad2DegZero) {
    EXPECT_FLOAT_EQ(RAD2DEG(0.0f), 0.0f);
}

TEST(MathMacros, Rad2DegPi) {
    EXPECT_NEAR(RAD2DEG(PI), 180.0f, 1e-5f);
}

TEST(MathMacros, Rad2Deg2Pi) {
    EXPECT_NEAR(RAD2DEG(2.0f * PI), 360.0f, 1e-5f);
}

TEST(MathMacros, FloatCompareExactEqual) {
    EXPECT_TRUE(floatCompare(1.0f, 1.0f));
}

TEST(MathMacros, FloatCompareVeryDifferent) {
    EXPECT_FALSE(floatCompare(1.0f, 100.0f));
}

TEST(MathMacros, FloatCompareNearZero) {
    EXPECT_TRUE(floatCompare(0.0f, 0.0f));
    EXPECT_TRUE(floatCompare(FLT_EPSILON * 0.5f, 0.0f));
}

// -----------------------------------------------------------------------------
// FastSqrt
// -----------------------------------------------------------------------------

TEST(MathFastSqrt, PerfectSquare) {
    EXPECT_FLOAT_EQ(Math::FastSqrt(16.0), 4.0);
}

TEST(MathFastSqrt, Zero) {
    EXPECT_FLOAT_EQ(Math::FastSqrt(0.0), 0.0);
}

TEST(MathFastSqrt, One) {
    EXPECT_FLOAT_EQ(Math::FastSqrt(1.0), 1.0);
}

// -----------------------------------------------------------------------------
// NormalizeAngle
// -----------------------------------------------------------------------------

TEST(MathNormalizeAngle, ZeroReturnsZero) {
    EXPECT_FLOAT_EQ(Math::NormalizeAngle(0.0f), 0.0f);
}

TEST(MathNormalizeAngle, OneEightyReturnsOneEighty) {
    EXPECT_FLOAT_EQ(Math::NormalizeAngle(180.0f), 180.0f);
}

TEST(MathNormalizeAngle, NegoneEightyReturnsNegoneEighty) {
    EXPECT_FLOAT_EQ(Math::NormalizeAngle(-180.0f), -180.0f);
}

TEST(MathNormalizeAngle, FourFiftyWrapsToNinety) {
    EXPECT_NEAR(Math::NormalizeAngle(450.0f), 90.0f, 1e-5f); // std::remainder wraps to [0, 90]
}

TEST(MathNormalizeAngle, NegativeFourFiftyWraps) {
    EXPECT_NEAR(Math::NormalizeAngle(-450.0f), -90.0f, 1e-5f);
}

TEST(MathNormalizeAngle, ThreeSixtyWrapsToZero) {
    EXPECT_NEAR(Math::NormalizeAngle(360.0f), 0.0f, 1e-5f);
}

TEST(MathNormalizeAngle, NaNReturnsZero) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FLOAT_EQ(Math::NormalizeAngle(nan), 0.0f);
}

TEST(MathNormalizeAngle, InfReturnsZero) {
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(Math::NormalizeAngle(inf), 0.0f);
    EXPECT_FLOAT_EQ(Math::NormalizeAngle(-inf), 0.0f);
}

// -----------------------------------------------------------------------------
// SinCos
// -----------------------------------------------------------------------------

TEST(MathSinCos, ZeroRadians) {
    float sine = 0.0f, cosine = 0.0f;
    Math::SinCos(0.0f, &sine, &cosine);
    EXPECT_NEAR(sine, 0.0f, 1e-6f);
    EXPECT_NEAR(cosine, 1.0f, 1e-6f);
}

TEST(MathSinCos, HalfPi) {
    float sine = 0.0f, cosine = 0.0f;
    Math::SinCos(PI / 2.0f, &sine, &cosine);
    EXPECT_NEAR(sine, 1.0f, 1e-6f);
    EXPECT_NEAR(cosine, 0.0f, 1e-6f);
}

TEST(MathSinCos, Pi) {
    float sine = 0.0f, cosine = 0.0f;
    Math::SinCos(PI, &sine, &cosine);
    EXPECT_NEAR(sine, 0.0f, 1e-6f);
    EXPECT_NEAR(cosine, -1.0f, 1e-6f);
}

// -----------------------------------------------------------------------------
// ClampAngles
// -----------------------------------------------------------------------------

TEST(MathClampAngles, ClampsPitchToRange) {
    Vec3 v(100.0f, 0.0f, 0.0f);
    Math::ClampAngles(v);
    EXPECT_FLOAT_EQ(v.x, 89.0f);
}

TEST(MathClampAngles, ClampsNegativePitch) {
    Vec3 v(-100.0f, 0.0f, 0.0f);
    Math::ClampAngles(v);
    EXPECT_FLOAT_EQ(v.x, -89.0f);
}

TEST(MathClampAngles, NormalizesYaw) {
    Vec3 v(0.0f, 450.0f, 0.0f);
    Math::ClampAngles(v);
    EXPECT_NEAR(v.y, 90.0f, 1e-5f);
}

TEST(MathClampAngles, ZerosRoll) {
    Vec3 v(0.0f, 0.0f, 45.0f);
    Math::ClampAngles(v);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(MathClampAngles, WithinRangeUnchanged) {
    Vec3 v(45.0f, 90.0f, 5.0f);
    Math::ClampAngles(v);
    EXPECT_FLOAT_EQ(v.x, 45.0f);
    EXPECT_FLOAT_EQ(v.y, 90.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

// -----------------------------------------------------------------------------
// VectorAngles (forward direction -> pitch/yaw)
// -----------------------------------------------------------------------------

TEST(MathVectorAngles, ForwardPositiveX) {
    Vec3 forward(1.0f, 0.0f, 0.0f);
    Vec3 angles;
    Math::VectorAngles(forward, angles);
    EXPECT_NEAR(angles.x, 0.0f, 1e-4f); // pitch
    EXPECT_NEAR(angles.y, 0.0f, 1e-4f); // yaw
}

TEST(MathVectorAngles, ForwardNegativeX) {
    Vec3 forward(-1.0f, 0.0f, 0.0f);
    Vec3 angles;
    Math::VectorAngles(forward, angles);
    EXPECT_NEAR(angles.y, 180.0f, 1e-4f); // yaw = 180
}

TEST(MathVectorAngles, StraightUpPositiveZ) {
    Vec3 forward(0.0f, 0.0f, 1.0f);
    Vec3 angles;
    Math::VectorAngles(forward, angles);
    EXPECT_FLOAT_EQ(angles.x, 270.0f); // pitch = 270 looking up
    EXPECT_FLOAT_EQ(angles.y, 0.0f);
}

TEST(MathVectorAngles, StraightDownNegativeZ) {
    Vec3 forward(0.0f, 0.0f, -1.0f);
    Vec3 angles;
    Math::VectorAngles(forward, angles);
    EXPECT_FLOAT_EQ(angles.x, 90.0f); // pitch = 90 looking down
    EXPECT_FLOAT_EQ(angles.y, 0.0f);
}

// -----------------------------------------------------------------------------
// AngleVectors (angles -> directions)
// -----------------------------------------------------------------------------

TEST(MathAngleVectors, ThreeParamZeroAnglesToForward) {
    Vec3 forward;
    Math::AngleVectors(Vec3(0.0f, 0.0f, 0.0f), &forward);
    EXPECT_NEAR(forward.x, 1.0f, 1e-5f);
    EXPECT_NEAR(forward.y, 0.0f, 1e-5f);
    EXPECT_NEAR(forward.z, 0.0f, 1e-5f);
}

TEST(MathAngleVectors, ThreeParamNullForwardNoCrash) {
    // Should not crash when forward is nullptr
    Math::AngleVectors(Vec3(45.0f, 90.0f, 0.0f), nullptr);
    // No crash = pass
    SUCCEED();
}

TEST(MathAngleVectors, SixParamAllDirections) {
    Vec3 forward, right, up;
    Math::AngleVectors(Vec3(0.0f, 0.0f, 0.0f), &forward, &right, &up);
    // Forward = +x
    EXPECT_NEAR(forward.x, 1.0f, 1e-5f);
    // Right = -y (Source engine convention)
    EXPECT_NEAR(right.x, 0.0f, 1e-5f);
    EXPECT_NEAR(right.y, -1.0f, 1e-5f);
    // Up = +z
    EXPECT_NEAR(up.z, 1.0f, 1e-5f);
}

TEST(MathAngleVectors, SixParamNullRightUp) {
    Vec3 forward;
    Math::AngleVectors(Vec3(0.0f, 0.0f, 0.0f), &forward, nullptr, nullptr);
    EXPECT_NEAR(forward.x, 1.0f, 1e-5f);
}

// -----------------------------------------------------------------------------
// VectorAngles / AngleVectors Round-Trip
// -----------------------------------------------------------------------------

TEST(MathAngleRoundTrip, ForwardToAnglesBackToForward) {
    const Vec3 original(0.5f, 0.7f, -0.2f);
    // Normalize the test direction
    Vec3 dir = original;
    dir.Normalize();

    Vec3 angles;
    Math::VectorAngles(dir, angles);

    Vec3 result;
    Math::AngleVectors(angles, &result);

    EXPECT_NEAR(result.x, dir.x, 1e-5f);
    EXPECT_NEAR(result.y, dir.y, 1e-5f);
    EXPECT_NEAR(result.z, dir.z, 1e-5f);
}

TEST(MathAngleRoundTrip, AnglesToVectorsBackToAngles) {
    const Vec3 originalAngles(30.0f, 45.0f, 0.0f);
    Vec3 forward;
    Math::AngleVectors(originalAngles, &forward);

    Vec3 resultAngles;
    Math::VectorAngles(forward, resultAngles);

    EXPECT_NEAR(resultAngles.x, originalAngles.x, 1e-4f);
    EXPECT_NEAR(resultAngles.y, originalAngles.y, 1e-4f);
}

// -----------------------------------------------------------------------------
// CalcAngle
// -----------------------------------------------------------------------------

TEST(MathCalcAngle, SourceEqualsDestinationReturnsZero) {
    const Vec3 src(100.0f, 100.0f, 100.0f);
    const Vec3 dst(100.0f, 100.0f, 100.0f);
    const Vec3 angles = Math::CalcAngle(src, dst, true);
    EXPECT_NEAR(angles.x, 0.0f, 1e-4f);
    EXPECT_NEAR(angles.y, 0.0f, 1e-4f);
}

TEST(MathCalcAngle, StraightForwardOnGround) {
    const Vec3 src(0.0f, 0.0f, 0.0f);
    const Vec3 dst(100.0f, 0.0f, 0.0f);
    const Vec3 angles = Math::CalcAngle(src, dst, true);
    EXPECT_NEAR(angles.x, 0.0f, 1e-4f);
    EXPECT_NEAR(angles.y, 0.0f, 1e-4f);
}

TEST(MathCalcAngle, StraightUp) {
    // Two points with same x,y but dst higher Z -> pitch should be negative
    const Vec3 src(0.0f, 0.0f, 0.0f);
    const Vec3 dst(0.0f, 0.0f, 100.0f);
    const Vec3 angles = Math::CalcAngle(src, dst, false); // no clamp to see raw value

    // In Source engine: pitch is inverted (up is -90, down is +90)
    EXPECT_LT(angles.x, -75.0f); // nearly straight up
    EXPECT_NEAR(angles.y, 0.0f, 1e-4f);
}

TEST(MathCalcAngle, WithClampDefaultsToTrue) {
    const Vec3 src(0.0f, 0.0f, 0.0f);
    const Vec3 dst(100.0f, 0.0f, 0.0f);
    const Vec3 angles = Math::CalcAngle(src, dst); // no clamp arg -> defaults to true
    EXPECT_FLOAT_EQ(angles.z, 0.0f); // roll clamped to 0
}

TEST(MathCalcAngle, NoClamp) {
    const Vec3 src(0.0f, 0.0f, 0.0f);
    const Vec3 dst(100.0f, 0.0f, 0.0f);
    const Vec3 angles = Math::CalcAngle(src, dst, false);
    EXPECT_FLOAT_EQ(angles.z, 0.0f); // z is always 0 anyway in the function
}

// -----------------------------------------------------------------------------
// CalcFov
// -----------------------------------------------------------------------------

TEST(MathCalcFov, SameAnglesAreZeroFov) {
    const Vec3 src(0.0f, 0.0f, 0.0f);
    const Vec3 dst(0.0f, 0.0f, 0.0f);
    EXPECT_NEAR(Math::CalcFov(src, dst), 0.0f, 1e-5f);
}

TEST(MathCalcFov, OrthogonalAngles) {
    // Zeros -> (1,0,0), 90yaw -> (0,1,0)... wait
    // AngleVectors(0,0,0) -> forward (1,0,0)
    // AngleVectors(0,90,0) -> forward (0,1,0)
    // Dot = 0*0 + 1*0 + 0*0 = 0 -> acos(0) = pi/2 = 90 deg
    const Vec3 src(0.0f, 0.0f, 0.0f);
    const Vec3 dst(0.0f, 90.0f, 0.0f);
    EXPECT_NEAR(Math::CalcFov(src, dst), 90.0f, 1e-4f);
}

TEST(MathCalcFov, OppositeAnglesAre180) {
    const Vec3 src(0.0f, 0.0f, 0.0f);
    const Vec3 dst(0.0f, 180.0f, 0.0f);
    EXPECT_NEAR(Math::CalcFov(src, dst), 180.0f, 1e-4f);
}

// -----------------------------------------------------------------------------
// VectorTransform
// -----------------------------------------------------------------------------

TEST(MathVectorTransform, IdentityMatrix) {
    const Vec3 input(1.0f, 2.0f, 3.0f);
    const matrix3x4_t identity = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };
    Vec3 output;
    Math::VectorTransform(input, identity, output);
    EXPECT_FLOAT_EQ(output.x, input.x);
    EXPECT_FLOAT_EQ(output.y, input.y);
    EXPECT_FLOAT_EQ(output.z, input.z);
}

TEST(MathVectorTransform, TranslationOnly) {
    const Vec3 input(5.0f, 10.0f, 15.0f);
    const matrix3x4_t translate = {
        {1.0f, 0.0f, 0.0f, 100.0f},
        {0.0f, 1.0f, 0.0f, 200.0f},
        {0.0f, 0.0f, 1.0f, 300.0f}
    };
    Vec3 output;
    Math::VectorTransform(input, translate, output);
    EXPECT_FLOAT_EQ(output.x, 105.0f);
    EXPECT_FLOAT_EQ(output.y, 210.0f);
    EXPECT_FLOAT_EQ(output.z, 315.0f);
}

// -----------------------------------------------------------------------------
// RemapVal and RemapValClamped
// -----------------------------------------------------------------------------

TEST(MathRemap, LinearRemapMidpoint) {
    // [0, 10] -> [0, 100], val = 5 -> 50
    EXPECT_FLOAT_EQ(Math::RemapVal(5.0f, 0.0f, 10.0f, 0.0f, 100.0f), 50.0f);
}

TEST(MathRemap, LinearRemapEndpoint) {
    EXPECT_FLOAT_EQ(Math::RemapVal(0.0f, 0.0f, 10.0f, 0.0f, 100.0f), 0.0f);
    EXPECT_FLOAT_EQ(Math::RemapVal(10.0f, 0.0f, 10.0f, 0.0f, 100.0f), 100.0f);
}

TEST(MathRemap, ExtrapolationAllowed) {
    // Value outside [A,B] -> extrapolate
    const float r = Math::RemapVal(15.0f, 0.0f, 10.0f, 0.0f, 100.0f);
    EXPECT_FLOAT_EQ(r, 150.0f);
}

TEST(MathRemap, AzeroBzeroValGreater) {
    // A == B -> if val >= B return D else C
    // val(10.0f) >= B(0.0f) -> D(200.0f)
    EXPECT_FLOAT_EQ(Math::RemapVal(10.0f, 0.0f, 0.0f, 0.0f, 200.0f), 200.0f);
}

TEST(MathRemap, AzeroBzeroValLess) {
    // val(-1.0f) >= B(0.0f)? no -> C(100.0f)
    EXPECT_FLOAT_EQ(Math::RemapVal(-1.0f, 0.0f, 0.0f, 100.0f, 200.0f), 100.0f);
}

TEST(MathRemapClamped, Midpoint) {
    EXPECT_FLOAT_EQ(Math::RemapValClamped(5.0f, 0.0f, 10.0f, 0.0f, 100.0f), 50.0f);
}

TEST(MathRemapClamped, ClampsToOutputRange) {
    // val at 15 mapped to [0,100] clamps at 100
    EXPECT_FLOAT_EQ(Math::RemapValClamped(15.0f, 0.0f, 10.0f, 0.0f, 100.0f), 100.0f);
    EXPECT_FLOAT_EQ(Math::RemapValClamped(-5.0f, 0.0f, 10.0f, 0.0f, 100.0f), 0.0f);
}

TEST(MathRemapClamped, AzeroBzeroGreater) {
    // A == B -> val >= B ? D : C
    EXPECT_FLOAT_EQ(Math::RemapValClamped(5.0f, 0.0f, 0.0f, 0.0f, 200.0f), 200.0f);
}

// -----------------------------------------------------------------------------
// VelocityToAngles
// -----------------------------------------------------------------------------

TEST(MathVelocityToAngles, PositiveX) {
    const Vec3 vel(100.0f, 0.0f, 0.0f);
    const Vec3 angles = Math::VelocityToAngles(vel);
    EXPECT_NEAR(angles.y, 0.0f, 1e-4f);
    EXPECT_NEAR(angles.x, 0.0f, 1e-4f);
}

TEST(MathVelocityToAngles, PositiveY) {
    const Vec3 vel(0.0f, 100.0f, 0.0f);
    const Vec3 angles = Math::VelocityToAngles(vel);
    EXPECT_NEAR(angles.y, 90.0f, 1e-4f);
    EXPECT_NEAR(angles.x, 0.0f, 1e-4f);
}

TEST(MathVelocityToAngles, StraightUpIs270) {
    const Vec3 vel(0.0f, 0.0f, 10.0f);
    const Vec3 angles = Math::VelocityToAngles(vel);
    EXPECT_FLOAT_EQ(angles.x, 270.0f); // special case: z > 0 -> pitch = 270
}

TEST(MathVelocityToAngles, StraightDownIs90) {
    const Vec3 vel(0.0f, 0.0f, -10.0f);
    const Vec3 angles = Math::VelocityToAngles(vel);
    EXPECT_FLOAT_EQ(angles.x, 90.0f); // special case: z <= 0 -> pitch = 90
}

// -----------------------------------------------------------------------------
// MatrixSetColumn
// -----------------------------------------------------------------------------

TEST(MathMatrixSetColumn, SetColumnZero) {
    matrix3x4_t m = {};
    Math::MatrixSetColumn(Vec3(1.0f, 2.0f, 3.0f), 0, m);
    EXPECT_FLOAT_EQ(m[0][0], 1.0f);
    EXPECT_FLOAT_EQ(m[1][0], 2.0f);
    EXPECT_FLOAT_EQ(m[2][0], 3.0f);
}

TEST(MathMatrixSetColumn, SetColumnThree) {
    matrix3x4_t m = {};
    Math::MatrixSetColumn(Vec3(10.0f, 20.0f, 30.0f), 3, m);
    EXPECT_FLOAT_EQ(m[0][3], 10.0f);
    EXPECT_FLOAT_EQ(m[1][3], 20.0f);
    EXPECT_FLOAT_EQ(m[2][3], 30.0f);
}

// -----------------------------------------------------------------------------
// AngleMatrix / MatrixAngles Round-Trip
// -----------------------------------------------------------------------------

TEST(MathMatrixRoundTrip, ZeroAngles) {
    const Vec3 angles(0.0f, 0.0f, 0.0f);
    matrix3x4_t m = {};
    Math::AngleMatrix(angles, m);

    float result[3] = {};
    Math::MatrixAngles(m, result);

    EXPECT_NEAR(result[0], 0.0f, 1e-5f);
    EXPECT_NEAR(result[1], 0.0f, 1e-5f);
    EXPECT_NEAR(result[2], 0.0f, 1e-5f);
}

TEST(MathMatrixRoundTrip, NinetyYaw) {
    const Vec3 angles(0.0f, 90.0f, 0.0f);
    matrix3x4_t m = {};
    Math::AngleMatrix(angles, m);

    float result[3] = {};
    Math::MatrixAngles(m, result);

    EXPECT_NEAR(result[0], 0.0f, 1e-4f);
    EXPECT_NEAR(result[1], 90.0f, 1e-4f);
    EXPECT_NEAR(result[2], 0.0f, 1e-4f);
}

TEST(MathMatrixRoundTrip, FortyFivePitch) {
    const Vec3 angles(45.0f, 0.0f, 0.0f);
    matrix3x4_t m = {};
    Math::AngleMatrix(angles, m);

    float result[3] = {};
    Math::MatrixAngles(m, result);

    EXPECT_NEAR(result[0], 45.0f, 1e-4f);
    EXPECT_NEAR(result[1], 0.0f, 1e-4f);
    EXPECT_NEAR(result[2], 0.0f, 1e-4f);
}

TEST(MathMatrixRoundTrip, FullOrientation) {
    const Vec3 angles(30.0f, -45.0f, 15.0f);
    matrix3x4_t m = {};
    Math::AngleMatrix(angles, m);

    float result[3] = {};
    Math::MatrixAngles(m, result);

    EXPECT_NEAR(result[0], 30.0f, 1e-3f);
    EXPECT_NEAR(result[1], -45.0f, 1e-3f);
    EXPECT_NEAR(result[2], 15.0f, 1e-3f);
}

// -----------------------------------------------------------------------------
// RotateTriangle
// -----------------------------------------------------------------------------

TEST(MathRotateTriangle, ZeroRotationPreservesPositions) {
    std::array<Vec2, 3> pts = { Vec2(0.0f, 0.0f), Vec2(10.0f, 0.0f), Vec2(5.0f, 10.0f) };
    const auto original = pts;
    Math::RotateTriangle(pts, 0.0f);
    for (int i = 0; i < 3; ++i) {
        EXPECT_NEAR(pts[i].x, original[i].x, 1e-5f);
        EXPECT_NEAR(pts[i].y, original[i].y, 1e-5f);
    }
}

TEST(MathRotateTriangle, NinetyDegreeRotatesAroundCentroid) {
    std::array<Vec2, 3> pts = { Vec2(0.0f, 0.0f), Vec2(10.0f, 0.0f), Vec2(0.0f, 10.0f) };
    // Centroid at (10/3, 10/3) ≈ (3.333, 3.333)
    // After 90 deg rotation around centroid, the shape is rotated
    const Vec2 centroidBefore = (pts[0] + pts[1] + pts[2]) / 3.0f;
    Math::RotateTriangle(pts, 90.0f);
    const Vec2 centroidAfter = (pts[0] + pts[1] + pts[2]) / 3.0f;
    EXPECT_NEAR(centroidBefore.x, centroidAfter.x, 1e-4f);
    EXPECT_NEAR(centroidBefore.y, centroidAfter.y, 1e-4f);
}

TEST(MathRotateTriangle, ThreeSixtyReturnsToOriginal) {
    std::array<Vec2, 3> pts = { Vec2(0.0f, 0.0f), Vec2(10.0f, 0.0f), Vec2(5.0f, 10.0f) };
    const auto original = pts;
    Math::RotateTriangle(pts, 360.0f);
    for (int i = 0; i < 3; ++i) {
        EXPECT_NEAR(pts[i].x, original[i].x, 1e-3f);
        EXPECT_NEAR(pts[i].y, original[i].y, 1e-3f);
    }
}

// -----------------------------------------------------------------------------
// RayToOBB (slab-based Ray vs Oriented Bounding Box)
// -----------------------------------------------------------------------------

TEST(MathRayToOBB, HitUnitCubeCenter) {
    // Ray from (-2,0,0) pointing at (+1,0,0) hits cube centered at (0,0,0)
    const Vec3 origin(-2.0f, 0.0f, 0.0f);
    const Vec3 direction(1.0f, 0.0f, 0.0f);
    const Vec3 position(0.0f, 0.0f, 0.0f);
    const Vec3 min(-0.5f, -0.5f, -0.5f);
    const Vec3 max(0.5f, 0.5f, 0.5f);

    // Identity orientation matrix
    const matrix3x4_t orientation = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };

    EXPECT_TRUE(Math::RayToOBB(origin, direction, position, min, max, orientation));
}

TEST(MathRayToOBB, MissBehindRay) {
    // Ray from some point pointing away from the cube
    const Vec3 origin(5.0f, 0.0f, 0.0f);
    const Vec3 direction(1.0f, 0.0f, 0.0f);
    const Vec3 position(0.0f, 0.0f, 0.0f);
    const Vec3 min(-0.5f, -0.5f, -0.5f);
    const Vec3 max(0.5f, 0.5f, 0.5f);

    const matrix3x4_t orientation = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };

    EXPECT_FALSE(Math::RayToOBB(origin, direction, position, min, max, orientation));
}

TEST(MathRayToOBB, MissYOffset) {
    // Ray aiming along x but offset too high in y
    const Vec3 origin(-2.0f, 2.0f, 0.0f);
    const Vec3 direction(1.0f, 0.0f, 0.0f);
    const Vec3 position(0.0f, 0.0f, 0.0f);
    const Vec3 min(-0.5f, -0.5f, -0.5f);
    const Vec3 max(0.5f, 0.5f, 0.5f);

    const matrix3x4_t orientation = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };

    EXPECT_FALSE(Math::RayToOBB(origin, direction, position, min, max, orientation));
}

TEST(MathRayToOBB, RayInsideBoxAlwaysHits) {
    // Ray starts inside the box
    const Vec3 origin(0.0f, 0.0f, 0.0f);
    const Vec3 direction(1.0f, 0.0f, 0.0f);
    const Vec3 position(0.0f, 0.0f, 0.0f);
    const Vec3 min(-0.5f, -0.5f, -0.5f);
    const Vec3 max(0.5f, 0.5f, 0.5f);

    const matrix3x4_t orientation = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };

    EXPECT_TRUE(Math::RayToOBB(origin, direction, position, min, max, orientation));
}

TEST(MathRayToOBB, RotatedBoxHit) {
    // Box rotated 45 deg around Z. Ray from (-2,0,0) along +x hits the corner.
    // When rotated 45 deg, a face corner extends into x at ~0.7.  Ray hits.
    const Vec3 origin(-2.0f, 0.0f, 0.0f);
    const Vec3 direction(1.0f, 0.0f, 0.0f);
    const Vec3 position(0.0f, 0.0f, 0.0f);
    const Vec3 min(-0.5f, -0.5f, -0.5f);
    const Vec3 max(0.5f, 0.5f, 0.5f);

    // 45 deg rotation around Z
    const float s = std::sinf(DEG2RAD(45.0f));
    const float c = std::cosf(DEG2RAD(45.0f));
    matrix3x4_t rotation = {
        {c, -s, 0.0f, 0.0f},
        {s,  c, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };

    EXPECT_TRUE(Math::RayToOBB(origin, direction, position, min, max, rotation));
}

TEST(MathRayToOBB, ParallelToFaceMiss) {
    // Ray runs along Y, parallel to a box face in the X direction
    // Box is at (0,0,0), ray is at x=2 heading along +Y
    const Vec3 origin(2.0f, -5.0f, 0.0f);
    const Vec3 direction(0.0f, 1.0f, 0.0f);
    const Vec3 position(0.0f, 0.0f, 0.0f);
    const Vec3 min(-0.5f, -0.5f, -0.5f);
    const Vec3 max(0.5f, 0.5f, 0.5f);

    const matrix3x4_t orientation = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };

    EXPECT_FALSE(Math::RayToOBB(origin, direction, position, min, max, orientation));
}

// -----------------------------------------------------------------------------
// VectorRotate
// -----------------------------------------------------------------------------

TEST(MathVectorRotate, IdentityMatrixPreservesVector) {
    const Vec3 input(1.0f, 2.0f, 3.0f);
    const matrix3x4_t identity = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };
    Vec3 output;
    Math::VectorRotate(const_cast<Vec3&>(input), identity, output); // in1 is non-const ref
    EXPECT_FLOAT_EQ(output.x, input.x);
    EXPECT_FLOAT_EQ(output.y, input.y);
    EXPECT_FLOAT_EQ(output.z, input.z);
}

TEST(MathVectorRotate, Rotate90YawAroundZ) {
    // 90 deg CCW: (1,0,0) -> (0,1,0)
    const float s = std::sinf(DEG2RAD(90.0f));
    const float c = std::cosf(DEG2RAD(90.0f));
    const matrix3x4_t rot90Z = {
        {c, -s, 0.0f, 0.0f},
        {s,  c, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };
    Vec3 input(1.0f, 0.0f, 0.0f);
    Vec3 output;
    Math::VectorRotate(input, rot90Z, output);
    EXPECT_NEAR(output.x, 0.0f, 1e-5f);
    EXPECT_NEAR(output.y, 1.0f, 1e-5f);
    EXPECT_NEAR(output.z, 0.0f, 1e-5f);
}

// -----------------------------------------------------------------------------
// VMatrix::As3x4
// -----------------------------------------------------------------------------

TEST(VMatrix, DefaultConstruction) {
    VMatrix mat;
}
