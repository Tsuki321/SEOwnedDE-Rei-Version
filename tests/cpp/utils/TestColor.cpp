#include <gtest/gtest.h>

#include "Utils/Color/Color.h"
#include "Utils/Utils.h"

#include <cmath>
#include <string>

// -----------------------------------------------------------------------------
// Color_t — toHexStr / toHexStrW
// -----------------------------------------------------------------------------

TEST(ColorStruct, ToHexStrRed) {
    const Color_t red{ 255, 0, 0, 255 };
    EXPECT_EQ(red.toHexStr(), std::string("FF0000FF"));
}

TEST(ColorStruct, ToHexStrGreen) {
    const Color_t green{ 0, 255, 0, 255 };
    EXPECT_EQ(green.toHexStr(), std::string("00FF00FF"));
}

TEST(ColorStruct, ToHexStrBlue) {
    const Color_t blue{ 0, 0, 255, 255 };
    EXPECT_EQ(blue.toHexStr(), std::string("0000FFFF"));
}

TEST(ColorStruct, ToHexStrWhite) {
    const Color_t white{ 255, 255, 255, 255 };
    EXPECT_EQ(white.toHexStr(), std::string("FFFFFFFF"));
}

TEST(ColorStruct, ToHexStrBlack) {
    const Color_t black{ 0, 0, 0, 255 };
    EXPECT_EQ(black.toHexStr(), std::string("000000FF"));
}

TEST(ColorStruct, ToHexStrAlphaZero) {
    const Color_t clear{ 255, 255, 255, 0 };
    EXPECT_EQ(clear.toHexStr(), std::string("FFFFFF00"));
}

TEST(ColorStruct, ToHexStrSingleDigitHex) {
    const Color_t c{ 10, 10, 10, 10 };
    EXPECT_EQ(c.toHexStr(), std::string("0A0A0A0A"));
}

TEST(ColorStruct, ToHexStrDefaultIsBlackTransparent) {
    const Color_t default_color;
    EXPECT_EQ(default_color.toHexStr(), std::string("00000000"));
}

TEST(ColorStruct, ToHexStrWIsSameAsHexStrContent) {
    const Color_t c{ 170, 187, 204, 221 }; // AA, BB, CC, DD
    const std::wstring expected(L"AABBCCDD");
    EXPECT_EQ(c.toHexStrW(), expected);
}

TEST(UtilsStringConversion, Utf8RoundTripPreservesContent) {
    const std::string utf8 = "prefix \xE4\xBD\xA0\xE5\xA5\xBD";
    const auto wide = Utils::ConvertUtf8ToWide(utf8);

    EXPECT_EQ(wide, std::wstring(L"prefix \u4F60\u597D"));
    EXPECT_EQ(Utils::ConvertWideToUTF8(wide), utf8);
}

TEST(UtilsStringConversion, EmbeddedNullsDoNotTruncate) {
    const std::string utf8{ 'A', '\0', 'B' };
    const std::wstring expectedWide{ L'A', L'\0', L'B' };
    const auto wide = Utils::ConvertUtf8ToWide(utf8);

    EXPECT_EQ(wide, expectedWide);
    EXPECT_EQ(Utils::ConvertWideToUTF8(wide), utf8);
}

// -----------------------------------------------------------------------------
// Colors Namespace Constants
// -----------------------------------------------------------------------------

TEST(ColorsNamespace, Red) {
    EXPECT_EQ(Colors::RED.r, 255u);
    EXPECT_EQ(Colors::RED.g, 0u);
    EXPECT_EQ(Colors::RED.b, 0u);
    EXPECT_EQ(Colors::RED.a, 255u);
}

TEST(ColorsNamespace, Green) {
    EXPECT_EQ(Colors::GREEN.r, 0u);
    EXPECT_EQ(Colors::GREEN.g, 255u);
    EXPECT_EQ(Colors::GREEN.b, 0u);
    EXPECT_EQ(Colors::GREEN.a, 255u);
}

TEST(ColorsNamespace, Blue) {
    EXPECT_EQ(Colors::BLUE.r, 0u);
    EXPECT_EQ(Colors::BLUE.g, 0u);
    EXPECT_EQ(Colors::BLUE.b, 255u);
    EXPECT_EQ(Colors::BLUE.a, 255u);
}

TEST(ColorsNamespace, White) {
    EXPECT_EQ(Colors::WHITE.r, 255u);
    EXPECT_EQ(Colors::WHITE.g, 255u);
    EXPECT_EQ(Colors::WHITE.b, 255u);
    EXPECT_EQ(Colors::WHITE.a, 255u);
}

TEST(ColorsNamespace, Black) {
    EXPECT_EQ(Colors::BLACK.r, 0u);
    EXPECT_EQ(Colors::BLACK.g, 0u);
    EXPECT_EQ(Colors::BLACK.b, 0u);
    EXPECT_EQ(Colors::BLACK.a, 255u);
}

// -----------------------------------------------------------------------------
// ColorUtils::ToFloat
// -----------------------------------------------------------------------------

TEST(ColorUtilsToFloat, ZeroIsZero) {
    EXPECT_FLOAT_EQ(ColorUtils::ToFloat(0), 0.0f);
}

TEST(ColorUtilsToFloat, 255IsOne) {
    EXPECT_FLOAT_EQ(ColorUtils::ToFloat(255), 1.0f);
}

TEST(ColorUtilsToFloat, 127IsApproxHalf) {
    EXPECT_NEAR(ColorUtils::ToFloat(127), 127.0f / 255.0f, 1e-6f);
}

TEST(ColorUtilsToFloat, 128IsApproxHalf) {
    EXPECT_NEAR(ColorUtils::ToFloat(128), 128.0f / 255.0f, 1e-6f);
}

// -----------------------------------------------------------------------------
// ColorUtils::ToDWORD
// -----------------------------------------------------------------------------

TEST(ColorUtilsToDWORD, RedToDWORD) {
    // Packed as: RR GG BB AA (big-endian bytes in the DWORD)
    // But MSVC DWORD is little-endian. The code does:
    //   (r << 24) | (g << 16) | (b << 8) | a
    // So r is in the MSB, a in the LSB of the 32-bit value.
    // In hex: 0xRRGGBBAA
    const unsigned long packed = ColorUtils::ToDWORD(Color_t{ 255, 0, 0, 255 });
    EXPECT_EQ(packed, 0xFF0000FFu);
}

TEST(ColorUtilsToDWORD, GreenToDWORD) {
    const unsigned long packed = ColorUtils::ToDWORD(Color_t{ 0, 255, 0, 255 });
    EXPECT_EQ(packed, 0x00FF00FFu);
}

TEST(ColorUtilsToDWORD, BlueToDWORD) {
    const unsigned long packed = ColorUtils::ToDWORD(Color_t{ 0, 0, 255, 255 });
    EXPECT_EQ(packed, 0x0000FFFFu);
}

TEST(ColorUtilsToDWORD, MixedColor) {
    // r=0xAA, g=0xBB, b=0xCC, a=0xDD -> 0xAABBCCDD
    const unsigned long packed = ColorUtils::ToDWORD(Color_t{ 0xAA, 0xBB, 0xCC, 0xDD });
    EXPECT_EQ(packed, 0xAABBCCDDu);
}

TEST(ColorUtilsToDWORD, AllZero) {
    const unsigned long packed = ColorUtils::ToDWORD(Color_t{ 0, 0, 0, 0 });
    EXPECT_EQ(packed, 0u);
}

// -----------------------------------------------------------------------------
// ColorUtils::Rainbow
// -----------------------------------------------------------------------------

TEST(ColorUtilsRainbow, AlphaAlways255) {
    const Color_t r = ColorUtils::Rainbow(0.0f);
    EXPECT_EQ(r.a, 255u);
}

TEST(ColorUtilsRainbow, ProducesVaryingColors) {
    const Color_t a = ColorUtils::Rainbow(0.0f);
    const Color_t b = ColorUtils::Rainbow(1.0f);
    // The colors should differ in at least one channel
    const bool different = (a.r != b.r) || (a.g != b.g) || (a.b != b.b);
    EXPECT_TRUE(different);
}

TEST(ColorUtilsRainbow, RateChangesCycleSpeed) {
    // Rate doubles the phase progression
    const Color_t r1 = ColorUtils::Rainbow(1.0f, 1.0f);
    const Color_t r2 = ColorUtils::Rainbow(0.5f, 2.0f);
    // Same phase * rate product -> same color
    EXPECT_EQ(r1.r, r2.r);
    EXPECT_EQ(r1.g, r2.g);
    EXPECT_EQ(r1.b, r2.b);
}

TEST(ColorUtilsRainbow, AllValuesInRange) {
    // Test many offsets to ensure colors always stay in byte range
    for (float offset = 0.0f; offset < 10.0f; offset += 0.05f) {
        const Color_t c = ColorUtils::Rainbow(offset);
        EXPECT_GE(c.r, 0u) << "r out of range at offset " << offset;
        EXPECT_LE(c.r, 255u) << "r out of range at offset " << offset;
        EXPECT_GE(c.g, 0u);
        EXPECT_LE(c.g, 255u);
        EXPECT_GE(c.b, 0u);
        EXPECT_LE(c.b, 255u);
    }
}

// -----------------------------------------------------------------------------
// ColorUtils::HSLToRGB
// -----------------------------------------------------------------------------

TEST(ColorUtilsHSLToRGB, Red) {
    // Hue = 0, Sat = 1, Light = 0.5 -> pure red
    const Color_t c = ColorUtils::HSLToRGB(0.0f, 1.0f, 0.5f);
    EXPECT_EQ(c.r, 255u);
    EXPECT_EQ(c.g, 0u);
    EXPECT_EQ(c.b, 0u);
    EXPECT_EQ(c.a, 255u);
}

TEST(ColorUtilsHSLToRGB, Black) {
    // Light = 0 -> black regardless of hue/saturation
    const Color_t c = ColorUtils::HSLToRGB(0.5f, 1.0f, 0.0f);
    EXPECT_EQ(c.r, 0u);
    EXPECT_EQ(c.g, 0u);
    EXPECT_EQ(c.b, 0u);
}

TEST(ColorUtilsHSLToRGB, White) {
    // Light = 1 -> white regardless of hue/saturation
    const Color_t c = ColorUtils::HSLToRGB(0.3f, 0.0f, 1.0f);
    EXPECT_EQ(c.r, 255u);
    EXPECT_EQ(c.g, 255u);
    EXPECT_EQ(c.b, 255u);
}

TEST(ColorUtilsHSLToRGB, GraySaturationZero) {
    // Saturation = 0 -> gray at whatever lightness
    const Color_t c = ColorUtils::HSLToRGB(0.7f, 0.0f, 0.5f);
    // All three channels should be equal (gray)
    EXPECT_EQ(c.r, c.g);
    EXPECT_EQ(c.g, c.b);
}

TEST(ColorUtilsHSLToRGB, HuesAreDistinct) {
    // Different hues should give different colors
    const Color_t red = ColorUtils::HSLToRGB(0.0f, 1.0f, 0.5f);
    const Color_t green = ColorUtils::HSLToRGB(120.0f / 360.0f, 1.0f, 0.5f);
    const Color_t blue = ColorUtils::HSLToRGB(240.0f / 360.0f, 1.0f, 0.5f);

    // They should be different from each other
    EXPECT_FALSE(red.r == green.r && red.g == green.g && red.b == green.b);
    EXPECT_FALSE(green.r == blue.r && green.g == blue.g && green.b == blue.b);
    EXPECT_FALSE(red.r == blue.r && red.g == blue.g && red.b == blue.b);
}

TEST(ColorUtilsHSLToRGB, HueWrapsAround) {
    // Hue = 1.0 == Hue = 0.0 (wraps via the [0,1] normalization)
    const Color_t a = ColorUtils::HSLToRGB(0.0f, 1.0f, 0.5f);
    const Color_t b = ColorUtils::HSLToRGB(1.0f, 1.0f, 0.5f); // wraps to 0
    EXPECT_EQ(a.r, b.r);
    EXPECT_EQ(a.g, b.g);
    EXPECT_EQ(a.b, b.b);
}

// -----------------------------------------------------------------------------
// ColorUtils::Mult
// -----------------------------------------------------------------------------

TEST(ColorUtilsMult, MultiplyByOnePreservesColor) {
    const Color_t input{ 100, 150, 200, 255 };
    const Color_t result = ColorUtils::Mult(input, 1.0f);
    EXPECT_EQ(result.r, 100u);
    EXPECT_EQ(result.g, 150u);
    EXPECT_EQ(result.b, 200u);
    EXPECT_EQ(result.a, 255u);
}

TEST(ColorUtilsMult, MultiplyByZeroReturnsBlack) {
    const Color_t input{ 100, 150, 200, 255 };
    const Color_t result = ColorUtils::Mult(input, 0.0f);
    EXPECT_EQ(result.r, 0u);
    EXPECT_EQ(result.g, 0u);
    EXPECT_EQ(result.b, 0u);
    EXPECT_EQ(result.a, 255u); // alpha preserved
}

TEST(ColorUtilsMult, MultiplyByTwoDoublesAndClamps) {
    const Color_t input{ 200, 100, 50, 255 };
    const Color_t result = ColorUtils::Mult(input, 2.0f);
    // 200*2=400 -> clamp to 255, 100*2=200, 50*2=100
    EXPECT_EQ(result.r, 255u);
    EXPECT_EQ(result.g, 200u);
    EXPECT_EQ(result.b, 100u);
    EXPECT_EQ(result.a, 255u);
}

TEST(ColorUtilsMult, MultiplyByHalf) {
    const Color_t input{ 100, 200, 50, 128 };
    const Color_t result = ColorUtils::Mult(input, 0.5f);
    EXPECT_EQ(result.r, 50u);
    EXPECT_EQ(result.g, 100u);
    EXPECT_EQ(result.b, 25u);
    EXPECT_EQ(result.a, 128u); // alpha preserved
}
