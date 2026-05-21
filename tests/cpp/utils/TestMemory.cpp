#include <gtest/gtest.h>

#include "Utils/Assert/Assert.h"
#include "Utils/Memory/Memory.h"

#include <array>
#include <cstddef>
#include <stdexcept>

TEST(MemoryPatternToBytes, ParsesHexAndWildcards)
{
	const auto bytes = Memory::PatternToBytes("48 8B ? ? 0D");

	ASSERT_EQ(bytes.size(), 5u);
	EXPECT_EQ(bytes[0], 0x48);
	EXPECT_EQ(bytes[1], 0x8B);
	EXPECT_EQ(bytes[2], -1);
	EXPECT_EQ(bytes[3], -1);
	EXPECT_EQ(bytes[4], 0x0D);
}

TEST(MemoryPatternToBytes, HandlesTrailingWildcardSafely)
{
	const auto bytes = Memory::PatternToBytes("48 8B ?");

	ASSERT_EQ(bytes.size(), 3u);
	EXPECT_EQ(bytes[0], 0x48);
	EXPECT_EQ(bytes[1], 0x8B);
	EXPECT_EQ(bytes[2], -1);
}

TEST(MemoryFindSignature, FindsMatchAtLastPossibleOffset)
{
	const std::array<std::byte, 6> bytes = {
		std::byte{0x11}, std::byte{0x22}, std::byte{0x33},
		std::byte{0x44}, std::byte{0x55}, std::byte{0x66}
	};

	const auto result = Memory::FindSignature(bytes.data(), bytes.size(), "44 55 66");

	ASSERT_NE(result, 0u);
	EXPECT_EQ(result, reinterpret_cast<std::uintptr_t>(bytes.data() + 3));
}

TEST(MemoryFindSignature, ReturnsZeroWhenPatternIsMissing)
{
	const std::array<std::byte, 4> bytes = {
		std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}, std::byte{0xDD}
	};

	const auto result = Memory::FindSignature(bytes.data(), bytes.size(), "11 22 33");

	EXPECT_EQ(result, 0u);
}

TEST(AssertFatal, ThrowsInUnitTestMode)
{
	EXPECT_THROW({ AssertFatal(false); }, std::runtime_error);
}
