#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Memory
{
	std::uintptr_t FindSignature(const char *szModule, const char *szPattern);
	PVOID FindInterface(const char *szModule, const char *szObject);
	std::vector<int> PatternToBytes(const char *pattern);
	std::uintptr_t FindSignature(const std::byte *image_bytes, size_t image_size, const char *szPattern);

	inline void* GetVFunc(void* instance, size_t index)
	{
		const auto vtable = *static_cast<void***>(instance);
		return vtable[index];
	}

	inline std::uintptr_t RelToAbs(const std::uintptr_t address)
	{
		return *reinterpret_cast<std::int32_t*>(address + 0x3) + address + 0x7;
	}
}
