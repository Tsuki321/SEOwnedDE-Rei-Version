#include <gtest/gtest.h>

#include "Utils/Vector/Vector.h"

#include <cstddef>
#include <cmath>
#include <cfloat>
#include <type_traits>

// -----------------------------------------------------------------------------
// Vec3 — Construction & Assignment
// -----------------------------------------------------------------------------

TEST(Vec3Construction, DefaultConstructorIsZero) {
    const Vec3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3Construction, ComponentConstructorStoresValues) {
    const Vec3 v(1.0f, -2.5f, 3.14f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, -2.5f);
    EXPECT_FLOAT_EQ(v.z, 3.14f);
}

TEST(Vec3Construction, FloatPtrConstructorCopiesElements) {
    float arr[3] = { 7.0f, 8.0f, 9.0f };
    const Vec3 v(arr);
    EXPECT_FLOAT_EQ(v.x, 7.0f);
    EXPECT_FLOAT_EQ(v.y, 8.0f);
    EXPECT_FLOAT_EQ(v.z, 9.0f);
}

TEST(Vec3Construction, ConstFloatPtrConstructorCopiesElements) {
    const float arr[3] = { 10.0f, 11.0f, 12.0f };
    const Vec3 v(arr);
    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 11.0f);
    EXPECT_FLOAT_EQ(v.z, 12.0f);
}

TEST(Vec3Construction, CopyConstructorDeepCopies) {
    const Vec3 original(4.0f, 5.0f, 6.0f);
    const Vec3 copy(original);
    EXPECT_FLOAT_EQ(copy.x, 4.0f);
    EXPECT_FLOAT_EQ(copy.y, 5.0f);
    EXPECT_FLOAT_EQ(copy.z, 6.0f);
}

TEST(Vec3Construction, CopyAssignmentReturnsSelf) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    const Vec3 other(4.0f, 5.0f, 6.0f);
    v = other;
    EXPECT_FLOAT_EQ(v.x, 4.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 6.0f);
}

TEST(Vec3Construction, ZeroSetsAllComponentsToZero) {
    Vec3 v(99.0f, 99.0f, 99.0f);
    v.Zero();
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3ABI, PreservesSourceThreeFloatLayout) {
    EXPECT_EQ(sizeof(Vec3), sizeof(float) * 3);
    EXPECT_EQ(alignof(Vec3), alignof(float));
    EXPECT_EQ(offsetof(Vec3, x), static_cast<std::size_t>(0));
    EXPECT_EQ(offsetof(Vec3, y), sizeof(float));
    EXPECT_EQ(offsetof(Vec3, z), sizeof(float) * 2);
    EXPECT_TRUE(std::is_standard_layout_v<Vec3>);
    EXPECT_TRUE(std::is_nothrow_default_constructible_v<Vec3>);
    EXPECT_TRUE(std::is_nothrow_copy_constructible_v<Vec3>);
    EXPECT_TRUE(std::is_nothrow_copy_assignable_v<Vec3>);
}

// -----------------------------------------------------------------------------
// Vec3 — Element Access
// -----------------------------------------------------------------------------

TEST(Vec3ElementAccess, SubscriptReturnsCorrectElement) {
    const Vec3 v(3.0f, 4.0f, 5.0f);
    EXPECT_FLOAT_EQ(v[0], 3.0f);
    EXPECT_FLOAT_EQ(v[1], 4.0f);
    EXPECT_FLOAT_EQ(v[2], 5.0f);
}

TEST(Vec3ElementAccess, MutableSubscriptModifiesElement) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    v[0] = 10.0f;
    v[1] = 20.0f;
    v[2] = 30.0f;
    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 20.0f);
    EXPECT_FLOAT_EQ(v.z, 30.0f);
}

// -----------------------------------------------------------------------------
// Vec3 — Set / Init
// -----------------------------------------------------------------------------

TEST(Vec3Mutation, SetAssignsComponents) {
    Vec3 v;
    v.Set(10.0f, 20.0f, 30.0f);
    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 20.0f);
    EXPECT_FLOAT_EQ(v.z, 30.0f);
}

TEST(Vec3Mutation, SetZeroDefaults) {
    Vec3 v(5.0f, 5.0f, 5.0f);
    v.Set(); // all defaults to 0
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3Mutation, InitAssignsComponents) {
    Vec3 v;
    v.Init(7.0f, 8.0f, 9.0f);
    EXPECT_FLOAT_EQ(v.x, 7.0f);
    EXPECT_FLOAT_EQ(v.y, 8.0f);
    EXPECT_FLOAT_EQ(v.z, 9.0f);
}

// -----------------------------------------------------------------------------
// Vec3 — Compound Assignment (Vec3 operand)
// -----------------------------------------------------------------------------

TEST(Vec3CompoundAssign, Vec3Add) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    v += Vec3(4.0f, 5.0f, 6.0f);
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 7.0f);
    EXPECT_FLOAT_EQ(v.z, 9.0f);
}

TEST(Vec3CompoundAssign, Vec3Subtract) {
    Vec3 v(10.0f, 20.0f, 30.0f);
    v -= Vec3(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 9.0f);
    EXPECT_FLOAT_EQ(v.y, 18.0f);
    EXPECT_FLOAT_EQ(v.z, 27.0f);
}

TEST(Vec3CompoundAssign, Vec3Multiply) {
    Vec3 v(2.0f, 3.0f, 4.0f);
    v *= Vec3(5.0f, 6.0f, 7.0f);
    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 18.0f);
    EXPECT_FLOAT_EQ(v.z, 28.0f);
}

TEST(Vec3CompoundAssign, Vec3Divide) {
    Vec3 v(10.0f, 20.0f, 30.0f);
    v /= Vec3(2.0f, 4.0f, 5.0f);
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 6.0f);
}

// -----------------------------------------------------------------------------
// Vec3 — Compound Assignment (scalar operand)
// -----------------------------------------------------------------------------

TEST(Vec3CompoundAssign, ScalarAdd) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    v += 5.0f;
    EXPECT_FLOAT_EQ(v.x, 6.0f);
    EXPECT_FLOAT_EQ(v.y, 7.0f);
    EXPECT_FLOAT_EQ(v.z, 8.0f);
}

TEST(Vec3CompoundAssign, ScalarSubtract) {
    Vec3 v(10.0f, 20.0f, 30.0f);
    v -= 5.0f;
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 15.0f);
    EXPECT_FLOAT_EQ(v.z, 25.0f);
}

TEST(Vec3CompoundAssign, ScalarMultiply) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    v *= 3.0f;
    EXPECT_FLOAT_EQ(v.x, 3.0f);
    EXPECT_FLOAT_EQ(v.y, 6.0f);
    EXPECT_FLOAT_EQ(v.z, 9.0f);
}

TEST(Vec3CompoundAssign, ScalarDivide) {
    Vec3 v(10.0f, 20.0f, 30.0f);
    v /= 2.0f;
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 10.0f);
    EXPECT_FLOAT_EQ(v.z, 15.0f);
}

// -----------------------------------------------------------------------------
// Vec3 — Binary Arithmetic (Vec3 operand)
// -----------------------------------------------------------------------------

TEST(Vec3Binary, Vec3AddReturnsNew) {
    const Vec3 a(1.0f, 2.0f, 3.0f);
    const Vec3 b(4.0f, 5.0f, 6.0f);
    const Vec3 r = a + b;
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 7.0f);
    EXPECT_FLOAT_EQ(r.z, 9.0f);
    // originals unchanged
    EXPECT_FLOAT_EQ(a.x, 1.0f);
    EXPECT_FLOAT_EQ(b.x, 4.0f);
}

TEST(Vec3Binary, Vec3SubtractReturnsNew) {
    const Vec3 r = Vec3(10.0f, 20.0f, 30.0f) - Vec3(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(r.x, 9.0f);
    EXPECT_FLOAT_EQ(r.y, 18.0f);
    EXPECT_FLOAT_EQ(r.z, 27.0f);
}

TEST(Vec3Binary, Vec3MultiplyReturnsNew) {
    const Vec3 r = Vec3(2.0f, 3.0f, 4.0f) * Vec3(5.0f, 6.0f, 7.0f);
    EXPECT_FLOAT_EQ(r.x, 10.0f);
    EXPECT_FLOAT_EQ(r.y, 18.0f);
    EXPECT_FLOAT_EQ(r.z, 28.0f);
}

TEST(Vec3Binary, Vec3DivideReturnsNew) {
    const Vec3 r = Vec3(30.0f, 40.0f, 50.0f) / Vec3(3.0f, 4.0f, 5.0f);
    EXPECT_FLOAT_EQ(r.x, 10.0f);
    EXPECT_FLOAT_EQ(r.y, 10.0f);
    EXPECT_FLOAT_EQ(r.z, 10.0f);
}

// -----------------------------------------------------------------------------
// Vec3 — Binary Arithmetic (scalar operand)
// -----------------------------------------------------------------------------

TEST(Vec3Binary, ScalarAddReturnsNew) {
    const Vec3 r = Vec3(1.0f, 2.0f, 3.0f) + 10.0f;
    EXPECT_FLOAT_EQ(r.x, 11.0f);
    EXPECT_FLOAT_EQ(r.y, 12.0f);
    EXPECT_FLOAT_EQ(r.z, 13.0f);
}

TEST(Vec3Binary, ScalarSubtractReturnsNew) {
    const Vec3 r = Vec3(10.0f, 20.0f, 30.0f) - 5.0f;
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 15.0f);
    EXPECT_FLOAT_EQ(r.z, 25.0f);
}

TEST(Vec3Binary, ScalarMultiplyReturnsNew) {
    const Vec3 r = Vec3(3.0f, 4.0f, 5.0f) * 2.0f;
    EXPECT_FLOAT_EQ(r.x, 6.0f);
    EXPECT_FLOAT_EQ(r.y, 8.0f);
    EXPECT_FLOAT_EQ(r.z, 10.0f);
}

TEST(Vec3Binary, ScalarDivideReturnsNew) {
    const Vec3 r = Vec3(10.0f, 20.0f, 30.0f) / 2.0f;
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 10.0f);
    EXPECT_FLOAT_EQ(r.z, 15.0f);
}

// -----------------------------------------------------------------------------
// Vec3 — Magnitude
// -----------------------------------------------------------------------------

TEST(Vec3Magnitude, LengthAxisAligned) {
    const Vec3 v(3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.Length(), 5.0f);
}

TEST(Vec3Magnitude, LengthZeroIsZero) {
    const Vec3 v;
    EXPECT_FLOAT_EQ(v.Length(), 0.0f);
}

TEST(Vec3Magnitude, Length3DUnitCube) {
    const Vec3 v(1.0f, 1.0f, 1.0f);
    EXPECT_NEAR(v.Length(), std::sqrtf(3.0f), 1e-6f);
}

TEST(Vec3Magnitude, LengthSqrMatchesSquareOfLength) {
    const Vec3 v(3.0f, 4.0f, 5.0f);
    EXPECT_FLOAT_EQ(v.LengthSqr(), 50.0f);
    EXPECT_NEAR(v.LengthSqr(), v.Length() * v.Length(), 1e-6f);
}

TEST(Vec3Magnitude, Length2DIgnoresZ) {
    const Vec3 v(3.0f, 4.0f, 100.0f);
    EXPECT_FLOAT_EQ(v.Length2D(), 5.0f);
}

TEST(Vec3Magnitude, Length2DSqrIgnoresZ) {
    const Vec3 v(3.0f, 4.0f, 100.0f);
    EXPECT_FLOAT_EQ(v.Length2DSqr(), 25.0f);
}

// -----------------------------------------------------------------------------
// Vec3 — Normalize
// -----------------------------------------------------------------------------

TEST(Vec3Normalize, NormalizeProducesUnitVector) {
    Vec3 v(3.0f, 0.0f, 4.0f);
    const float origLen = v.Normalize();
    EXPECT_FLOAT_EQ(origLen, 5.0f);
    EXPECT_NEAR(v.Length(), 1.0f, 1e-6f);
}

TEST(Vec3Normalize, NormalizeZeroVectorProducesZero) {
    Vec3 v(0.0f, 0.0f, 0.0f);
    const float len = v.Normalize();
    EXPECT_FLOAT_EQ(len, 0.0f);
    // Normalized zero vector: all components should be zero
    // because normalization uses fl_Length * (1 / (epsilon + 0)) = 0
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3Normalize, NormalizeInPlaceSameAsNormalize) {
    Vec3 a(3.0f, 4.0f, 0.0f);
    Vec3 b(3.0f, 4.0f, 0.0f);
    const float lenA = a.Normalize();
    const float lenB = b.NormalizeInPlace();
    EXPECT_FLOAT_EQ(lenA, lenB);
    EXPECT_FLOAT_EQ(a.x, b.x);
    EXPECT_FLOAT_EQ(a.y, b.y);
    EXPECT_FLOAT_EQ(a.z, b.z);
}

// -----------------------------------------------------------------------------
// Vec3 — Distance
// -----------------------------------------------------------------------------

TEST(Vec3Distance, DistToZero) {
    const Vec3 a(0.0f, 0.0f, 0.0f);
    const Vec3 b(3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.DistTo(b), 5.0f);
}

TEST(Vec3Distance, DistToSamePointIsZero) {
    const Vec3 a(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(a.DistTo(a), 0.0f);
}

TEST(Vec3Distance, DistToSqrMatchesSquareOfDistTo) {
    const Vec3 a(1.0f, 2.0f, 3.0f);
    const Vec3 b(4.0f, 6.0f, 8.0f);
    EXPECT_FLOAT_EQ(a.DistToSqr(b), a.DistTo(b) * a.DistTo(b));
}

// -----------------------------------------------------------------------------
// Vec3 — Dot / Cross
// -----------------------------------------------------------------------------

TEST(Vec3Math, DotOrthogonalIsZero) {
    const Vec3 a(1.0f, 0.0f, 0.0f);
    const Vec3 b(0.0f, 1.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.Dot(b), 0.0f);
}

TEST(Vec3Math, DotParallelIsProduct) {
    const Vec3 a(3.0f, 0.0f, 0.0f);
    const Vec3 b(4.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.Dot(b), 12.0f);
}

TEST(Vec3Math, CrossRightHandProducesZ) {
    const Vec3 xAxis(1.0f, 0.0f, 0.0f);
    const Vec3 yAxis(0.0f, 1.0f, 0.0f);
    const Vec3 z = xAxis.Cross(yAxis);
    EXPECT_NEAR(z.x, 0.0f, 1e-6f);
    EXPECT_NEAR(z.y, 0.0f, 1e-6f);
    EXPECT_NEAR(z.z, 1.0f, 1e-6f);
}

TEST(Vec3Math, CrossSelfIsZero) {
    const Vec3 v(3.0f, 4.0f, 5.0f);
    const Vec3 r = v.Cross(v);
    EXPECT_NEAR(r.x, 0.0f, 1e-6f);
    EXPECT_NEAR(r.y, 0.0f, 1e-6f);
    EXPECT_NEAR(r.z, 0.0f, 1e-6f);
}

TEST(Vec3Math, CrossIsAntiCommutative) {
    const Vec3 a(1.0f, 2.0f, 3.0f);
    const Vec3 b(4.0f, 5.0f, 6.0f);
    const Vec3 ab = a.Cross(b);
    const Vec3 ba = b.Cross(a);
    EXPECT_FLOAT_EQ(ab.x, -ba.x);
    EXPECT_FLOAT_EQ(ab.y, -ba.y);
    EXPECT_FLOAT_EQ(ab.z, -ba.z);
}

// -----------------------------------------------------------------------------
// Vec3 — IsZero
// -----------------------------------------------------------------------------

TEST(Vec3Predicates, IsZeroTrueForZero) {
    const Vec3 v;
    EXPECT_TRUE(v.IsZero());
}

TEST(Vec3Predicates, IsZeroTrueWithinTolerance) {
    const Vec3 v(0.005f, -0.005f, 0.0f);
    EXPECT_TRUE(v.IsZero());
}

TEST(Vec3Predicates, IsZeroFalseAtBoundaryPositive) {
    const Vec3 v(0.01f, 0.0f, 0.0f);
    EXPECT_FALSE(v.IsZero());
}

TEST(Vec3Predicates, IsZeroFalseAtBoundaryNegative) {
    const Vec3 v(-0.01f, 0.0f, 0.0f);
    EXPECT_FALSE(v.IsZero());
}

// -----------------------------------------------------------------------------
// Vec3 — Scale
// -----------------------------------------------------------------------------

TEST(Vec3Scale, Double) {
    const Vec3 v(1.0f, 2.0f, 3.0f);
    const Vec3 r = v.Scale(2.0f);
    EXPECT_FLOAT_EQ(r.x, 2.0f);
    EXPECT_FLOAT_EQ(r.y, 4.0f);
    EXPECT_FLOAT_EQ(r.z, 6.0f);
}

TEST(Vec3Scale, Half) {
    const Vec3 v(2.0f, 4.0f, 8.0f);
    const Vec3 r = v.Scale(0.5f);
    EXPECT_FLOAT_EQ(r.x, 1.0f);
    EXPECT_FLOAT_EQ(r.y, 2.0f);
    EXPECT_FLOAT_EQ(r.z, 4.0f);
}

TEST(Vec3Scale, NegativeScale) {
    const Vec3 v(1.0f, 2.0f, 3.0f);
    const Vec3 r = v.Scale(-1.0f);
    EXPECT_FLOAT_EQ(r.x, -1.0f);
    EXPECT_FLOAT_EQ(r.y, -2.0f);
    EXPECT_FLOAT_EQ(r.z, -3.0f);
}

TEST(Vec3Scale, ZeroScale) {
    const Vec3 v(1.0f, 2.0f, 3.0f);
    const Vec3 r = v.Scale(0.0f);
    EXPECT_FLOAT_EQ(r.x, 0.0f);
    EXPECT_FLOAT_EQ(r.y, 0.0f);
    EXPECT_FLOAT_EQ(r.z, 0.0f);
    EXPECT_TRUE(r.IsZero());
}

// -----------------------------------------------------------------------------
// Vec3 — Edge Cases
// -----------------------------------------------------------------------------

TEST(Vec3EdgeCases, NegativeComponentLength) {
    const Vec3 v(-3.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.Length(), 3.0f);
}

TEST(Vec3EdgeCases, LargeMagnitude) {
    const Vec3 v(1e19f, 1e19f, 1e19f);
    // Should not overflow LengthSqr
    EXPECT_GT(v.LengthSqr(), 0.0f);
    EXPECT_TRUE(std::isfinite(v.Length()));
}

TEST(Vec3EdgeCases, NearEpsilonLength) {
    const Vec3 v(1e-20f, 0.0f, 0.0f);
    const float len = v.Length();
    EXPECT_NEAR(len, 0.0f, 1e-6f);
}

TEST(Vec3EdgeCases, CrossWithNegatives) {
    const Vec3 a(-1.0f, 2.0f, -3.0f);
    const Vec3 b(4.0f, -5.0f, 6.0f);
    const Vec3 r = a.Cross(b);
    // (2*6 - (-3)*(-5), (-3)*4 - (-1)*6, (-1)*(-5) - 2*4)
    // = (12 - 15, -12 - (-6), 5 - 8)
    // = (-3, -6, -3)
    EXPECT_FLOAT_EQ(r.x, -3.0f);
    EXPECT_FLOAT_EQ(r.y, -6.0f);
    EXPECT_FLOAT_EQ(r.z, -3.0f);
}

// =============================================================================
// Vec2 Tests
// =============================================================================

// -----------------------------------------------------------------------------
// Vec2 — Construction & Assignment
// -----------------------------------------------------------------------------

TEST(Vec2Construction, DefaultConstructorIsZero) {
    const Vec2 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST(Vec2Construction, ComponentConstructorStoresValues) {
    const Vec2 v(3.5f, -7.2f);
    EXPECT_FLOAT_EQ(v.x, 3.5f);
    EXPECT_FLOAT_EQ(v.y, -7.2f);
}

TEST(Vec2Construction, FloatPtrConstructorCopiesElements) {
    float arr[2] = { 5.0f, 6.0f };
    const Vec2 v(arr);
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 6.0f);
}

TEST(Vec2Construction, ConstFloatPtrConstructorCopiesElements) {
    const float arr[2] = { 8.0f, 9.0f };
    const Vec2 v(arr);
    EXPECT_FLOAT_EQ(v.x, 8.0f);
    EXPECT_FLOAT_EQ(v.y, 9.0f);
}

TEST(Vec2Construction, CopyConstructorDeepCopies) {
    const Vec2 original(10.0f, 20.0f);
    const Vec2 copy(original);
    EXPECT_FLOAT_EQ(copy.x, 10.0f);
    EXPECT_FLOAT_EQ(copy.y, 20.0f);
}

TEST(Vec2Construction, CopyAssignmentReturnsSelf) {
    Vec2 v(1.0f, 2.0f);
    const Vec2 other(100.0f, 200.0f);
    v = other;
    EXPECT_FLOAT_EQ(v.x, 100.0f);
    EXPECT_FLOAT_EQ(v.y, 200.0f);
}

TEST(Vec2ABI, PreservesSourceTwoFloatLayout) {
    EXPECT_EQ(sizeof(Vec2), sizeof(float) * 2);
    EXPECT_EQ(alignof(Vec2), alignof(float));
    EXPECT_EQ(offsetof(Vec2, x), static_cast<std::size_t>(0));
    EXPECT_EQ(offsetof(Vec2, y), sizeof(float));
    EXPECT_TRUE(std::is_standard_layout_v<Vec2>);
    EXPECT_TRUE(std::is_nothrow_default_constructible_v<Vec2>);
    EXPECT_TRUE(std::is_nothrow_copy_constructible_v<Vec2>);
    EXPECT_TRUE(std::is_nothrow_copy_assignable_v<Vec2>);
}

// -----------------------------------------------------------------------------
// Vec2 — Element Access
// -----------------------------------------------------------------------------

TEST(Vec2ElementAccess, SubscriptReturnsCorrectElement) {
    const Vec2 v(7.0f, 8.0f);
    EXPECT_FLOAT_EQ(v[0], 7.0f);
    EXPECT_FLOAT_EQ(v[1], 8.0f);
}

TEST(Vec2ElementAccess, MutableSubscriptModifiesElement) {
    Vec2 v;
    v[0] = 50.0f;
    v[1] = 60.0f;
    EXPECT_FLOAT_EQ(v.x, 50.0f);
    EXPECT_FLOAT_EQ(v.y, 60.0f);
}

// -----------------------------------------------------------------------------
// Vec2 — Set
// -----------------------------------------------------------------------------

TEST(Vec2Mutation, SetAssignsComponents) {
    Vec2 v;
    v.Set(15.0f, 25.0f);
    EXPECT_FLOAT_EQ(v.x, 15.0f);
    EXPECT_FLOAT_EQ(v.y, 25.0f);
}

TEST(Vec2Mutation, SetZeroDefaults) {
    Vec2 v(5.0f, 5.0f);
    v.Set(); // all defaults to 0
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

// -----------------------------------------------------------------------------
// Vec2 — Compound Assignment (Vec2 operand)
// -----------------------------------------------------------------------------

TEST(Vec2CompoundAssign, Vec2Add) {
    Vec2 v(1.0f, 2.0f);
    v += Vec2(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.x, 4.0f);
    EXPECT_FLOAT_EQ(v.y, 6.0f);
}

TEST(Vec2CompoundAssign, Vec2Subtract) {
    Vec2 v(10.0f, 20.0f);
    v -= Vec2(3.0f, 5.0f);
    EXPECT_FLOAT_EQ(v.x, 7.0f);
    EXPECT_FLOAT_EQ(v.y, 15.0f);
}

TEST(Vec2CompoundAssign, Vec2Multiply) {
    Vec2 v(3.0f, 4.0f);
    v *= Vec2(2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 6.0f);
    EXPECT_FLOAT_EQ(v.y, 12.0f);
}

TEST(Vec2CompoundAssign, Vec2Divide) {
    Vec2 v(24.0f, 30.0f);
    v /= Vec2(4.0f, 5.0f);
    EXPECT_FLOAT_EQ(v.x, 6.0f);
    EXPECT_FLOAT_EQ(v.y, 6.0f);
}

// -----------------------------------------------------------------------------
// Vec2 — Compound Assignment (scalar operand)
// -----------------------------------------------------------------------------

TEST(Vec2CompoundAssign, ScalarAdd) {
    Vec2 v(1.0f, 2.0f);
    v += 10.0f;
    EXPECT_FLOAT_EQ(v.x, 11.0f);
    EXPECT_FLOAT_EQ(v.y, 12.0f);
}

TEST(Vec2CompoundAssign, ScalarSubtract) {
    Vec2 v(20.0f, 30.0f);
    v -= 5.0f;
    EXPECT_FLOAT_EQ(v.x, 15.0f);
    EXPECT_FLOAT_EQ(v.y, 25.0f);
}

TEST(Vec2CompoundAssign, ScalarMultiply) {
    Vec2 v(3.0f, 4.0f);
    v *= 2.0f;
    EXPECT_FLOAT_EQ(v.x, 6.0f);
    EXPECT_FLOAT_EQ(v.y, 8.0f);
}

TEST(Vec2CompoundAssign, ScalarDivide) {
    Vec2 v(50.0f, 100.0f);
    v /= 2.0f;
    EXPECT_FLOAT_EQ(v.x, 25.0f);
    EXPECT_FLOAT_EQ(v.y, 50.0f);
}

// -----------------------------------------------------------------------------
// Vec2 — Binary Arithmetic (Vec2 operand)
// -----------------------------------------------------------------------------

TEST(Vec2Binary, Vec2AddReturnsNew) {
    const Vec2 r = Vec2(1.0f, 2.0f) + Vec2(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(r.x, 4.0f);
    EXPECT_FLOAT_EQ(r.y, 6.0f);
}

TEST(Vec2Binary, Vec2SubtractReturnsNew) {
    const Vec2 r = Vec2(10.0f, 15.0f) - Vec2(2.0f, 3.0f);
    EXPECT_FLOAT_EQ(r.x, 8.0f);
    EXPECT_FLOAT_EQ(r.y, 12.0f);
}

TEST(Vec2Binary, Vec2MultiplyReturnsNew) {
    const Vec2 r = Vec2(3.0f, 5.0f) * Vec2(2.0f, 4.0f);
    EXPECT_FLOAT_EQ(r.x, 6.0f);
    EXPECT_FLOAT_EQ(r.y, 20.0f);
}

TEST(Vec2Binary, Vec2DivideReturnsNew) {
    const Vec2 r = Vec2(60.0f, 80.0f) / Vec2(5.0f, 4.0f);
    EXPECT_FLOAT_EQ(r.x, 12.0f);
    EXPECT_FLOAT_EQ(r.y, 20.0f);
}

// -----------------------------------------------------------------------------
// Vec2 — Binary Arithmetic (scalar operand)
// -----------------------------------------------------------------------------

TEST(Vec2Binary, ScalarAddReturnsNew) {
    const Vec2 r = Vec2(1.0f, 2.0f) + 5.0f;
    EXPECT_FLOAT_EQ(r.x, 6.0f);
    EXPECT_FLOAT_EQ(r.y, 7.0f);
}

TEST(Vec2Binary, ScalarSubtractReturnsNew) {
    const Vec2 r = Vec2(10.0f, 20.0f) - 3.0f;
    EXPECT_FLOAT_EQ(r.x, 7.0f);
    EXPECT_FLOAT_EQ(r.y, 17.0f);
}

TEST(Vec2Binary, ScalarMultiplyReturnsNew) {
    const Vec2 r = Vec2(4.0f, 5.0f) * 3.0f;
    EXPECT_FLOAT_EQ(r.x, 12.0f);
    EXPECT_FLOAT_EQ(r.y, 15.0f);
}

TEST(Vec2Binary, ScalarDivideReturnsNew) {
    const Vec2 r = Vec2(60.0f, 80.0f) / 2.0f;
    EXPECT_FLOAT_EQ(r.x, 30.0f);
    EXPECT_FLOAT_EQ(r.y, 40.0f);
}

// -----------------------------------------------------------------------------
// Vec2 — Magnitude
// -----------------------------------------------------------------------------

TEST(Vec2Magnitude, Length3_4_5) {
    const Vec2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.Length(), 5.0f);
}

TEST(Vec2Magnitude, LengthZeroIsZero) {
    const Vec2 v;
    EXPECT_FLOAT_EQ(v.Length(), 0.0f);
}

TEST(Vec2Magnitude, LengthSqrMatchesSquare) {
    const Vec2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.LengthSqr(), 25.0f);
    EXPECT_NEAR(v.LengthSqr(), v.Length() * v.Length(), 1e-6f);
}

// -----------------------------------------------------------------------------
// Vec2 — Distance
// -----------------------------------------------------------------------------

TEST(Vec2Distance, DistTo) {
    const Vec2 a(0.0f, 0.0f);
    const Vec2 b(6.0f, 8.0f);
    EXPECT_FLOAT_EQ(a.DistTo(b), 10.0f);
}

TEST(Vec2Distance, DistToSameIsZero) {
    const Vec2 v(5.0f, 5.0f);
    EXPECT_FLOAT_EQ(v.DistTo(v), 0.0f);
}

TEST(Vec2Distance, DistToSqrMatchesSquareOfDistTo) {
    const Vec2 a(1.0f, 2.0f);
    const Vec2 b(4.0f, 6.0f);
    EXPECT_FLOAT_EQ(a.DistToSqr(b), a.DistTo(b) * a.DistTo(b));
}

// -----------------------------------------------------------------------------
// Vec2 — Dot
// -----------------------------------------------------------------------------

TEST(Vec2Math, DotOrthogonalIsZero) {
    const Vec2 xAxis(1.0f, 0.0f);
    const Vec2 yAxis(0.0f, 1.0f);
    EXPECT_FLOAT_EQ(xAxis.Dot(yAxis), 0.0f);
}

TEST(Vec2Math, DotParallelIsProduct) {
    const Vec2 a(3.0f, 0.0f);
    const Vec2 b(4.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.Dot(b), 12.0f);
}

TEST(Vec2Math, DotSelfIsLengthSqr) {
    const Vec2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.Dot(v), 25.0f);
}

// -----------------------------------------------------------------------------
// Vec2 — IsZero
// -----------------------------------------------------------------------------

TEST(Vec2Predicates, IsZeroTrueForZero) {
    const Vec2 v;
    EXPECT_TRUE(v.IsZero());
}

TEST(Vec2Predicates, IsZeroTrueWithinTolerance) {
    const Vec2 v(0.005f, -0.005f);
    EXPECT_TRUE(v.IsZero());
}

TEST(Vec2Predicates, IsZeroFalseAtBoundaryPositive) {
    const Vec2 v(0.01f, 0.0f);
    EXPECT_FALSE(v.IsZero());
}

TEST(Vec2Predicates, IsZeroFalseAtBoundaryNegative) {
    const Vec2 v(-0.01f, 0.0f);
    EXPECT_FALSE(v.IsZero());
}
