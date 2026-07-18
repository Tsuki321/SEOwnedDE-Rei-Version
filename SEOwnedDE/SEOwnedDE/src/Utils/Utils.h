#pragma once

#include "Assert/Assert.h"
#include "Color/Color.h"
#include "Config/Config.h"
#include "Storage/Storage.h"
#include "Hash/Hash.h"
#include "HookManager/HookManager.h"
#include "InterfaceManager/InterfaceManager.h"
#include "Math/Math.h"
#include "Memory/Memory.h"
#include "SignatureManager/SignatureManager.h"
#include "Singleton/Singleton.h"
#include "Vector/Vector.h"

#include <intrin.h>
#include <random>
#include <chrono>
#include <filesystem>
#include <deque>
#include <regex>
#include <limits>

namespace Utils
{
	inline std::wstring ConvertUtf8ToWide(const std::string &utf8)
	{
		if (utf8.empty() || utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return {};

		const int sourceSize = static_cast<int>(utf8.size());
		const int resultSize = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), sourceSize, nullptr, 0);

		if (resultSize <= 0)
			return {};

		std::wstring result(static_cast<std::size_t>(resultSize), L'\0');
		if (MultiByteToWideChar(CP_UTF8, 0, utf8.data(), sourceSize, result.data(), resultSize) != resultSize)
			return {};

		return result;
	}

	inline std::string ConvertWideToUTF8(const std::wstring &unicode)
	{
		if (unicode.empty() || unicode.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return {};

		const int sourceSize = static_cast<int>(unicode.size());
		const int resultSize = WideCharToMultiByte(CP_UTF8, 0, unicode.data(), sourceSize, nullptr, 0, nullptr, nullptr);

		if (resultSize <= 0)
			return {};

		std::string result(static_cast<std::size_t>(resultSize), '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, unicode.data(), sourceSize, result.data(), resultSize, nullptr, nullptr) != resultSize)
			return {};

		return result;
	}

    static int RandInt(int min, int max)
    {
        thread_local static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> distr(min, max);
        return distr(gen);
    }
}
