#include "Memory.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <Psapi.h>

typedef void *(*InstantiateInterfaceFn)();

struct InterfaceInit_t
{
	InstantiateInterfaceFn m_pInterface = nullptr;
	const char *m_pszInterfaceName = nullptr;
	InterfaceInit_t *m_pNextInterface = nullptr;
};

namespace
{
	struct ScanRegion
	{
		const std::byte *m_pBytes = nullptr;
		size_t m_nSize = 0;
	};

	struct ModuleScanCache
	{
		const std::byte *m_pImage = nullptr;
		size_t m_nImageSize = 0;
		std::vector<ScanRegion> m_vecExecutableRegions = {};
		std::unordered_map<std::string, std::uintptr_t> m_mapSignatures = {};
	};

	std::unordered_map<HMODULE, ModuleScanCache> g_mapModules = {};

	std::uintptr_t FindCompiledSignature(const std::byte *imageBytes, size_t imageSize, const std::vector<int> &patternBytes)
	{
		const size_t signatureSize = patternBytes.size();
		if (!imageBytes || signatureSize == 0 || signatureSize > imageSize)
			return 0;

		// Anchor memchr on the final concrete byte.  IDA signatures commonly
		// begin with 0x48, so the tail is generally more selective than the head.
		size_t anchorOffset = signatureSize;
		for (size_t i = 0; i < signatureSize; ++i)
		{
			if (patternBytes[i] != -1)
				anchorOffset = i;
		}

		if (anchorOffset == signatureSize)
			return reinterpret_cast<std::uintptr_t>(imageBytes);

		const auto anchorByte = static_cast<unsigned char>(patternBytes[anchorOffset]);
		const size_t candidateCount = imageSize - signatureSize + 1;
		const std::byte *search = imageBytes + anchorOffset;
		size_t remaining = candidateCount;

		while (remaining)
		{
			const auto found = static_cast<const std::byte *>(std::memchr(search, anchorByte, remaining));
			if (!found)
				return 0;

			const size_t candidate = static_cast<size_t>(found - imageBytes) - anchorOffset;
			bool matches = true;
			for (size_t j = 0; j < signatureSize; ++j)
			{
				if (patternBytes[j] != -1 && imageBytes[candidate + j] != static_cast<std::byte>(patternBytes[j]))
				{
					matches = false;
					break;
				}
			}

			if (matches)
				return reinterpret_cast<std::uintptr_t>(imageBytes + candidate);

			const size_t consumed = static_cast<size_t>(found - search) + 1;
			search += consumed;
			remaining -= consumed;
		}

		return 0;
	}

	void PopulateExecutableRegions(ModuleScanCache &cache)
	{
		if (!cache.m_pImage || cache.m_nImageSize < sizeof(IMAGE_DOS_HEADER))
			return;

		const auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(cache.m_pImage);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew < 0)
			return;

		const size_t ntOffset = static_cast<size_t>(dosHeader->e_lfanew);
		if (cache.m_nImageSize < sizeof(IMAGE_NT_HEADERS) || ntOffset > cache.m_nImageSize - sizeof(IMAGE_NT_HEADERS))
			return;

		const auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS *>(cache.m_pImage + ntOffset);
		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
			return;

		const auto sections = IMAGE_FIRST_SECTION(ntHeaders);
		const auto imageEnd = cache.m_pImage + cache.m_nImageSize;
		const auto sectionBytes = reinterpret_cast<const std::byte *>(sections);
		if (sectionBytes < cache.m_pImage || sectionBytes > imageEnd)
			return;

		const size_t availableSectionHeaders = static_cast<size_t>(imageEnd - sectionBytes) / sizeof(IMAGE_SECTION_HEADER);
		if (ntHeaders->FileHeader.NumberOfSections > availableSectionHeaders)
			return;

		cache.m_vecExecutableRegions.reserve(ntHeaders->FileHeader.NumberOfSections);
		for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i)
		{
			const auto &section = sections[i];
			if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE) || section.VirtualAddress >= cache.m_nImageSize)
				continue;

			size_t sectionSize = section.Misc.VirtualSize ? section.Misc.VirtualSize : section.SizeOfRawData;
			sectionSize = (std::min)(sectionSize, cache.m_nImageSize - section.VirtualAddress);
			if (sectionSize)
				cache.m_vecExecutableRegions.push_back({ cache.m_pImage + section.VirtualAddress, sectionSize });
		}
	}

	ModuleScanCache *GetModuleScanCache(HMODULE module)
	{
		if (const auto it = g_mapModules.find(module); it != g_mapModules.end())
			return &it->second;

		MODULEINFO moduleInfo = {};
		if (!GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo)) || !moduleInfo.SizeOfImage)
			return nullptr;

		ModuleScanCache cache = {};
		cache.m_pImage = reinterpret_cast<const std::byte *>(moduleInfo.lpBaseOfDll);
		cache.m_nImageSize = moduleInfo.SizeOfImage;
		PopulateExecutableRegions(cache);

		const auto result = g_mapModules.emplace(module, std::move(cache));
		return &result.first->second;
	}
}

std::vector<int> Memory::PatternToBytes(const char *pattern)
{
	std::vector<int> bytes{};
	if (!pattern)
		return bytes;

	const size_t len = strlen(pattern);
	bytes.reserve(len / 3 + 1);

	const char *const end = pattern + len;

	for (const char *current = pattern; current < end; )
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
	return FindCompiledSignature(image_bytes, image_size, pattern_bytes);
}

std::uintptr_t Memory::FindSignature(const char *szModule, const char *szPattern)
{
	if (!szModule || !szPattern)
		return 0;

	if (const auto hMod = GetModuleHandleA(szModule))
	{
#ifdef _DEBUG
#define DEBUG_SIG
#endif

		auto module = GetModuleScanCache(hMod);
		if (!module)
		{
#ifdef DEBUG_SIG
			MessageBox(nullptr, std::format("GetModuleInformation {} failed\n", szPattern).c_str(), "", 0);
#endif
			return {};
		}

		if (const auto it = module->m_mapSignatures.find(szPattern); it != module->m_mapSignatures.end())
			return it->second;

		const auto patternBytes = PatternToBytes(szPattern);
		for (const auto &region : module->m_vecExecutableRegions)
		{
			if (const auto result = FindCompiledSignature(region.m_pBytes, region.m_nSize, patternBytes))
			{
				module->m_mapSignatures.emplace(szPattern, result);
				return result;
			}
		}

		// Preserve the original whole-image behavior for data signatures and
		// unusual modules without conventional executable section metadata.
		if (const auto result = FindCompiledSignature(module->m_pImage, module->m_nImageSize, patternBytes))
		{
			module->m_mapSignatures.emplace(szPattern, result);
			return result;
		}

#if defined DEBUG_SIG
		//MessageBox(nullptr, std::format("find_ida_sig {} failed\n", szPattern).c_str(), "", 0);
#endif

		/// Byte sequence wasn't found
		module->m_mapSignatures.emplace(szPattern, 0);
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
