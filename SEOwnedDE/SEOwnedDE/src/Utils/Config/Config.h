#pragma once
#include <nlohmann/json.hpp>

#include "../Color/Color.h"

#include <vector>
#include <type_traits>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <string>
#include <cmath>
#include <limits>

namespace Config
{
	enum class EConfigVarType : unsigned char
	{
		Boolean,
		Integer,
		Float,
		Color,
		String
	};

	template <typename>
	inline constexpr bool UnsupportedConfigVarType = false;

	template <typename T>
	constexpr EConfigVarType GetConfigVarType() noexcept
	{
		using Type = std::remove_cv_t<std::remove_reference_t<T>>;

		if constexpr (std::is_same_v<Type, bool>)
			return EConfigVarType::Boolean;
		else if constexpr (std::is_same_v<Type, int>)
			return EConfigVarType::Integer;
		else if constexpr (std::is_same_v<Type, float>)
			return EConfigVarType::Float;
		else if constexpr (std::is_same_v<Type, Color_t>)
			return EConfigVarType::Color;
		else if constexpr (std::is_same_v<Type, std::string>)
			return EConfigVarType::String;
		else
		{
			static_assert(UnsupportedConfigVarType<Type>, "CFGVAR only supports bool, int, float, Color_t, and std::string");
			return EConfigVarType::Boolean;
		}
	}

	struct ConfigVarInitializer
	{
		const char *m_name{ nullptr };
		void *m_ptr{ nullptr };
		EConfigVarType m_type{ EConfigVarType::Boolean };
		bool m_no_save{ false };
		const char *m_legacy_name{ nullptr };
	};

	inline std::vector<ConfigVarInitializer> vars{};

	template <typename T>
	bool ReadNumber(const nlohmann::json& value, T& result)
	{
		if (!value.is_number())
			return false;
		const double number = value.get<double>();
		if (!std::isfinite(number)
			|| number < static_cast<double>(std::numeric_limits<T>::lowest())
			|| number > static_cast<double>(std::numeric_limits<T>::max()))
			return false;
		result = static_cast<T>(number);
		return true;
	}

	static void Save(const std::filesystem::path& path)
	{
		std::ofstream output_file(path);

		if (!output_file.is_open())
		{
			return;
		}

		nlohmann::json j = nlohmann::json::object();

		for (const auto &var : vars)
		{
			if (var.m_no_save)
			{
				continue;
			}

			auto &value = j[var.m_name];

			switch (var.m_type)
			{
				case EConfigVarType::Boolean:
					value = *static_cast<bool *>(var.m_ptr);
					break;
				case EConfigVarType::Integer:
					value = *static_cast<int *>(var.m_ptr);
					break;
				case EConfigVarType::Float:
					value = *static_cast<float *>(var.m_ptr);
					break;
				case EConfigVarType::Color:
				{
					const auto &clr = *static_cast<Color_t *>(var.m_ptr);
					value = { clr.r, clr.g, clr.b, clr.a };
					break;
				}
				case EConfigVarType::String:
					value = *static_cast<std::string *>(var.m_ptr);
					break;
			}
		}

		output_file << std::setw(4) << j;

		output_file.close();
	}

	static void Load(const std::filesystem::path& path)
	{
		std::ifstream input_file(path);

		if (!input_file.is_open())
		{
			return;
		}

		nlohmann::json j{};

		try
		{
			input_file >> j;
		}
		catch (const nlohmann::json::exception&)
		{
			input_file.close();
			return;
		}

		if (j.is_object())
		{
			// Assist used to be aim mode 3. Preserve its gentle keyed behavior
			// through Smooth while enabling the independent assist toggle.
			const auto mode = j.find("Aimbot_Hitscan_Aim_Type");
			int aimType = 1;
			if (mode != j.end() && ReadNumber(*mode, aimType))
			{
				if (aimType == 3)
					*mode = 2;
				else if (aimType < 0 || aimType > 2)
					*mode = 1;
			}
			if (!j.contains("Aimbot_Hitscan_AimAssist_Active"))
				j["Aimbot_Hitscan_AimAssist_Active"] = true;
		}

		for (const auto &var : vars)
		{
			if (var.m_no_save)
			{
				continue;
			}

			auto valueIt = j.find(var.m_name);
			if (valueIt == j.end() && var.m_legacy_name)
			{
				valueIt = j.find(var.m_legacy_name);
			}

			if (valueIt == j.end())
			{
				continue;
			}

			const auto &value = *valueIt;

			try
			{
				switch (var.m_type)
				{
					case EConfigVarType::Boolean:
						*static_cast<bool *>(var.m_ptr) = value.get<bool>();
						break;
					case EConfigVarType::Integer:
						ReadNumber(value, *static_cast<int *>(var.m_ptr));
						break;
					case EConfigVarType::Float:
						ReadNumber(value, *static_cast<float *>(var.m_ptr));
						break;
					case EConfigVarType::Color:
					{
						Color_t color{};
						if (ReadNumber(value.at(0), color.r) && ReadNumber(value.at(1), color.g)
							&& ReadNumber(value.at(2), color.b) && ReadNumber(value.at(3), color.a))
							*static_cast<Color_t *>(var.m_ptr) = color;
						break;
					}
					case EConfigVarType::String:
						*static_cast<std::string *>(var.m_ptr) = value.get<std::string>();
						break;
				}
			}
			catch (const nlohmann::json::exception&)
			{
				continue;
			}
		}

		input_file.close();
	}
}

#define CFGVAR(var, val) inline auto var{ val }; \
namespace configvar_initializers\
{\
	inline auto var##_initializer = []()\
	{\
		Config::vars.push_back(Config::ConfigVarInitializer{#var, &var, Config::GetConfigVarType<decltype(var)>(), false });\
		return true;\
	}();\
}

#define CFGVAR_NOSAVE(var, val) inline auto var{ val }; \
namespace configvar_initializers\
{\
	inline auto var##_initializer = []()\
	{\
		Config::vars.push_back(Config::ConfigVarInitializer{#var, &var, Config::GetConfigVarType<decltype(var)>(), true });\
		return true;\
	}();\
}

#define CFGVAR_MIGRATED(var, val, legacy_name) inline auto var{ val }; \
namespace configvar_initializers\
{\
	inline auto var##_initializer = []()\
	{\
		Config::vars.push_back(Config::ConfigVarInitializer{#var, &var, Config::GetConfigVarType<decltype(var)>(), false, legacy_name });\
		return true;\
	}();\
}
