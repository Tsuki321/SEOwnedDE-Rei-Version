#pragma once

#include <cstddef>
#include <cstdint>
#include <bit>

namespace SkinAttributes
{
	// The setter accepts float, but integer schema attributes use its raw bits.
	inline float EncodeInteger(std::uint32_t value) { return std::bit_cast<float>(value); }
}

// IDA verified x64 CEconItemAttribute. Its definition ID is a WORD; the
// following padding can contain nonzero bytes and is not part of the ID.
struct EconItemAttributeLayout
{
	void* m_pVTable = nullptr;
	std::uint16_t m_nDefinitionIndex = 0;
	std::uint16_t m_nPadding = 0;
	std::uint32_t m_nRawValue = 0;
	std::int32_t m_nRefundableCurrency = 0;

	bool Matches(std::uint16_t index, std::uint32_t rawValue) const
	{
		return m_nDefinitionIndex == index && m_nRawValue == rawValue;
	}
};

static_assert(sizeof(EconItemAttributeLayout) == 24);
static_assert(offsetof(EconItemAttributeLayout, m_nDefinitionIndex) == 8);
static_assert(offsetof(EconItemAttributeLayout, m_nRawValue) == 12);
