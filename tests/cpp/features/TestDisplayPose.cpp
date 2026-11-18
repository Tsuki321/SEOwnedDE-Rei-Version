#include <gtest/gtest.h>
#include "../../../SEOwnedDE/SEOwnedDE/src/App/Features/LagRecords/RenderedPoseClock.h"
#include "../../../SEOwnedDE/SEOwnedDE/src/App/Features/SkinChanger/RuntimeAttributeLayout.h"
#include <bit>
#include <limits>

TEST(RenderedPoseClock, VisibleBodyTickCancelsInterpolationExactly)
{
    RenderedPoseClock clock;
    clock.Capture(10.0f, 0.1f);
    EXPECT_NEAR(clock.PoseTime(), 9.9f, 0.0001f);
    EXPECT_EQ(clock.CommandTick(0.1f, 0.015f, -1), 667);
}

TEST(RenderedPoseClock, LocalPredictionAndNewNetworkSamplesCannotAdvanceTheVisiblePose)
{
    RenderedPoseClock clock;
    clock.Capture(10.0f, 0.1f);
    // New command ticks arrive before another frame is rendered. The displayed
    // body has not moved, even if prediction and network simulation have.
    for (int predictedTick = 668; predictedTick < 674; ++predictedTick)
        EXPECT_EQ(clock.CommandTick(0.1f, 0.015f, predictedTick), 667);
    clock.Capture(10.06f, 0.1f);
    EXPECT_EQ(clock.CommandTick(0.1f, 0.015f, -1), 671);
}

TEST(RenderedPoseClock, UpdatedLerpStillTargetsThePreviouslyVisibleBody)
{
    RenderedPoseClock clock;
    clock.Capture(10.0f, 0.1f);
    const int tick = clock.CommandTick(0.2f, 0.015f, -1);
    EXPECT_NEAR(tick * 0.015f - 0.2f, clock.PoseTime(), 0.0076f);
}

TEST(RenderedPoseClock, CaptureAdvancesAtLowAndHighFrameRates)
{
    for (float frameTime : {1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f})
    {
        RenderedPoseClock clock;
        clock.Capture(10.0f, 0.03f);
        for (int frame = 1; frame <= 30; ++frame)
        {
            const float previous = clock.PoseTime();
            clock.Capture(10.0f + frame * frameTime, 0.03f);
            EXPECT_GT(clock.PoseTime(), previous);
        }
    }
}

TEST(RenderedPoseClock, DisconnectAndInvalidTimesDoNotStampStaleShots)
{
    RenderedPoseClock clock;
    EXPECT_EQ(clock.CommandTick(0.1f, 0.015f, 123), 123);
    clock.Capture(10.0f, 0.1f);
    clock.Clear();
    EXPECT_EQ(clock.CommandTick(0.1f, 0.015f, 123), 123);
    clock.Capture(std::numeric_limits<float>::quiet_NaN(), 0.1f);
    EXPECT_EQ(clock.CommandTick(0.1f, 0.015f, 123), 123);
    clock.Capture(10.0f, 0.1f);
    EXPECT_EQ(clock.CommandTick(0.1f, 0.0f, 123), 123);
    clock.Capture(0.0f, 0.1f);
    EXPECT_EQ(clock.CommandTick(0.1f, 0.015f, 123), 123);
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
