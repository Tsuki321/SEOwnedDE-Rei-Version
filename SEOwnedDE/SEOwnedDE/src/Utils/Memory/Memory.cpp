#include "Memory.h"
#include <cstdlib>
#include <cstring>
#include <format>

typedef void *(*InstantiateInterfaceFn)();

struct InterfaceInit_t
{
	InstantiateInterfaceFn m_pInterface = nullptr;
	const char *m_pszInterfaceName = nullptr;
	InterfaceInit_t *m_pNextInterface = nullptr;
};

#include <vector>
#include <Psapi.h>

std::vector<int> Memory::PatternToBytes(const char *pattern)
{
	std::vector<int> bytes{};
	if (!pattern)
		return bytes;

	const auto start = pattern;
	const char *const end = pattern + strlen(pattern);

	for (const char *current = start; current < end; )
	{
		if (*current == ' ')
		{
			++current;
			continue;
		}

		if (*current == '?')
		{
			++current;
			if (current < end && *current == '?')
				++current;

			bytes.push_back(-1);
			continue;
		}

		char *next = nullptr;
		const auto value = std::strtoul(current, &next, 16);
		if (next == current)
		{
			++current;
			continue;
		}

		bytes.push_back(static_cast<int>(value));
		current = next;
	}

	return bytes;
}

std::uintptr_t Memory::FindSignature(const std::byte *image_bytes, size_t image_size, const char *szPattern)
{
	const auto pattern_bytes = PatternToBytes(szPattern);
	const auto signature_size = pattern_bytes.size();
	const int *signature_bytes = pattern_bytes.data();

	if (!image_bytes || signature_size == 0 || signature_size > image_size)
		return 0x0;

	for (size_t i = 0; i <= image_size - signature_size; ++i)
	{
		bool byte_sequence_found = true;

		for (size_t j = 0; j < signature_size; ++j)
		{
			if (image_bytes[i + j] != static_cast<std::byte>(signature_bytes[j]) && signature_bytes[j] != -1)
			{
				byte_sequence_found = false;
				break;
			}
		}

		if (byte_sequence_found)
			return reinterpret_cast<std::uintptr_t>(&image_bytes[i]);
	}

	return 0x0;
}

std::uintptr_t Memory::FindSignature(const char *szModule, const char *szPattern)
{
	if (const auto hMod = GetModuleHandleA(szModule))
	{
#ifdef _DEBUG
#define DEBUG_SIG
#endif

		/// Get module information to search in the given module
		MODULEINFO module_info;
		if (!GetModuleInformation(GetCurrentProcess(), hMod, &module_info, sizeof(MODULEINFO)))
		{
#ifdef DEBUG_SIG
			MessageBox(nullptr, std::format("GetModuleInformation {} failed\n", szPattern).c_str(), "", 0);
#endif
			return {};
		}

		/// The region where we will search for the byte sequence
		const auto image_size = module_info.SizeOfImage;

		/// Check if the image is faulty
		if (!image_size)
			return {};

		const auto image_bytes = reinterpret_cast<byte *>(hMod);
		const auto result = FindSignature(reinterpret_cast<const std::byte *>(image_bytes), image_size, szPattern);
		if (result)
			return result;

#if defined DEBUG_SIG
		//MessageBox(nullptr, std::format("find_ida_sig {} failed\n", szPattern).c_str(), "", 0);
#endif

		/// Byte sequence wasn't found
		return {};
	}

	return 0x0;
}

using CreateInterfaceFn = void*(*)(const char* pName, int* pReturnCode);

PVOID Memory::FindInterface(const char *szModule, const char *szObject)
{
	/*auto hmModule = GetModuleHandleA(szModule);

	if (!hmModule)
		return nullptr;

	auto dwCreateInterface = reinterpret_cast<std::uintptr_t>(GetProcAddress(hmModule, "CreateInterface"));
	auto dwShortJmp = dwCreateInterface + 0x5;
	auto dwJmp = (dwShortJmp + *reinterpret_cast<std::uintptr_t *>(dwShortJmp)) + 0x4;

	auto pList = **reinterpret_cast<InterfaceInit_t ***>(dwJmp + 0x6);

	while (pList)
	{
		if ((strstr(pList->m_pszInterfaceName, szObject) && (strlen(pList->m_pszInterfaceName) - strlen(szObject)) < 5))
			return pList->m_pInterface();

		pList = pList->m_pNextInterface;
	}

	return nullptr;*/

	const auto hModule = GetModuleHandleA(szModule);
	if (!hModule) { return nullptr; }

	const auto createFn = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hModule, "CreateInterface"));
	if (!createFn) { return nullptr; }

	return createFn(szObject, nullptr);
}
