#include <gtest/gtest.h>

#include "../../../SEOwnedDE/SEOwnedDE/src/App/Features/Rendering/AnimationSmoothing.h"
#include "../../../SEOwnedDE/SEOwnedDE/src/App/Features/SkinChanger/RuntimeAttributeLayout.h"

#include <bit>
#include <limits>

namespace
{
using Matrix = float[3][4]; // Same array type used by Source.
using Pose = AnimationSmoothing::Pose<Matrix, 2>;

void Identity(Matrix& m, float x = 0.0f)
{
    std::memset(&m, 0, sizeof(m));
    m[0][0] = m[1][1] = m[2][2] = 1.0f;
    m[0][3] = x;
}

struct DisplayPoseTest : testing::Test
{
    Pose pose;
    Matrix bones[2]{};
    float origin[3]{};

    void SetUp() override { Identity(bones[0]); Identity(bones[1]); }
    Matrix* Update(int frame, float now, float sim, std::uint64_t identity = 1, int model = 1)
    {
        return pose.Update(bones, 2, identity, model, frame, now, sim, 0.015f, origin);
    }
};
}

TEST_F(DisplayPoseTest, BlendsOnlyDisplayCopyAndCompletesAtSnapshotInterval)
{
    Update(1, 1.0f, 1.0f);
    bones[0][0][3] = 12.0f;
    EXPECT_FLOAT_EQ(Update(2, 1.03f, 1.03f)[0][0][3], 0.0f);
    auto output = Update(3, 1.045f, 1.03f);
    ASSERT_NE(output, nullptr);
    EXPECT_NEAR(output[0][0][3], 6.0f, 0.001f);
    EXPECT_FLOAT_EQ(bones[0][0][3], 12.0f); // Shot/historical input is untouched.
    EXPECT_FLOAT_EQ(Update(4, 1.07f, 1.03f)[0][0][3], 12.0f);
}

TEST_F(DisplayPoseTest, RepeatedMaterialPassDoesNotAdvanceTheBlend)
{
    Update(1, 1.0f, 1.0f);
    bones[0][0][3] = 12.0f;
    Update(2, 1.03f, 1.03f);
    const float first = Update(3, 1.045f, 1.03f)[0][0][3];
    EXPECT_FLOAT_EQ(Update(3, 1.045f, 1.03f)[0][0][3], first);
}

TEST_F(DisplayPoseTest, NewSnapshotStartsAtLastDisplayedPose)
{
    Update(1, 1.0f, 1.0f);
    bones[0][0][3] = 12.0f;
    Update(2, 1.03f, 1.03f);
    const float previous = Update(3, 1.045f, 1.03f)[0][0][3];
    bones[0][0][3] = 24.0f;
    EXPECT_FLOAT_EQ(Update(4, 1.06f, 1.06f)[0][0][3], previous);
}

TEST_F(DisplayPoseTest, ResetsOnTeleportReusedHandleModelChangeAndClockRewind)
{
    Update(1, 1.0f, 1.0f);
    bones[0][0][3] = origin[0] = 500.0f;
    EXPECT_FLOAT_EQ(Update(2, 1.03f, 1.03f)[0][0][3], 500.0f);
    bones[0][0][3] = 520.0f;
    EXPECT_FLOAT_EQ(Update(3, 1.06f, 1.06f, 2)[0][0][3], 520.0f);
    bones[0][0][3] = 540.0f;
    EXPECT_FLOAT_EQ(Update(4, 1.09f, 1.09f, 2, 2)[0][0][3], 540.0f);
    bones[0][0][3] = 560.0f;
    EXPECT_FLOAT_EQ(Update(1, 0.5f, 0.5f, 2, 2)[0][0][3], 560.0f);
}

TEST_F(DisplayPoseTest, ResetsAfterDormancyAndRejectsInvalidInputs)
{
    Update(1, 1.0f, 1.0f);
    bones[0][0][3] = 12.0f;
    EXPECT_FLOAT_EQ(Update(50, 2.0f, 2.0f)[0][0][3], 12.0f);
    EXPECT_EQ(Update(51, std::numeric_limits<float>::quiet_NaN(), 2.0f), nullptr);
    EXPECT_EQ(pose.Update(bones, 3, 1, 1, 52, 2.1f, 2.1f, 0.015f, origin), nullptr);
}

TEST(DisplayBone, HalfTurnPreservesScaleAndOrthogonalAxes)
{
    Matrix from{}, to{}, out{};
    Identity(from); Identity(to);
    to[0][0] = to[1][1] = -1.0f;
    AnimationSmoothing::BlendBone(from, to, 0.5f, out);
    for (int col = 0; col < 3; ++col)
    {
        float length = 0.0f;
        for (int row = 0; row < 3; ++row) length += out[row][col] * out[row][col];
        EXPECT_NEAR(length, 1.0f, 0.0001f);
    }
    EXPECT_NEAR(out[0][0], 0.0f, 0.0001f);
    EXPECT_NEAR(std::abs(out[1][0]), 1.0f, 0.0001f);
    to[0][0] = to[1][1] = to[2][2] = 2.0f;
    AnimationSmoothing::BlendBone(from, to, 0.5f, out);
    EXPECT_FLOAT_EQ(out[0][0], 1.5f);
    EXPECT_FLOAT_EQ(out[1][1], 1.5f);
    EXPECT_FLOAT_EQ(out[2][2], 1.5f);
}

TEST_F(DisplayPoseTest, BoneMissingFromPreviousLodUsesCurrentEngineTransform)
{
    bool valid[2]{true, false};
    pose.Update(bones, 2, 1, 1, 1, 1.0f, 1.0f, 0.015f, origin, valid);
    bones[1][0][3] = 30.0f;
    valid[1] = true;
    const auto output = pose.Update(bones, 2, 1, 1, 2, 1.03f, 1.03f, 0.015f, origin, valid);
    EXPECT_FLOAT_EQ(output[1][0][3], 30.0f);
}

TEST(SkinAttributeLayout, MatchesDefinitionRegardlessOfPadding)
{
    EconItemAttributeLayout attribute;
    attribute.m_nDefinitionIndex = 834;
    attribute.m_nPadding = 0xabcd;
    attribute.m_nRawValue = std::bit_cast<std::uint32_t>(150.0f);
    EXPECT_TRUE(attribute.Matches(834, std::bit_cast<std::uint32_t>(150.0f)));
    EXPECT_FALSE(attribute.Matches(725, attribute.m_nRawValue));
    EXPECT_FALSE(attribute.Matches(834, std::bit_cast<std::uint32_t>(151.0f)));
}

TEST(SkinAttributeLayout, SeedComparisonPreservesAllRawBitsIncludingNanPatterns)
{
    EconItemAttributeLayout attribute;
    attribute.m_nDefinitionIndex = 866;
    attribute.m_nRawValue = 0x7fc00001;
    EXPECT_TRUE(attribute.Matches(866, 0x7fc00001));
    EXPECT_FALSE(attribute.Matches(866, 0x7fc00002));
}

TEST(SkinAttributeLayout, IntegerSchemaValuesSurviveTheFloatSetterExactly)
{
    for (const std::uint32_t value : {1u, 150u, 16777217u, 0x7fffffffu})
        EXPECT_EQ(std::bit_cast<std::uint32_t>(SkinAttributes::EncodeInteger(value)), value);
}
