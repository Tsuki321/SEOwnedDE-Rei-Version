#pragma once
#include <nlohmann/json.hpp>

#include "../Color/Color.h"

#include <vector>
#include <type_traits>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <string>

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
	};

	inline std::vector<ConfigVarInitializer> vars{};

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

		for (const auto &var : vars)
		{
			if (var.m_no_save)
			{
				continue;
			}

			const auto valueIt = j.find(var.m_name);
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
						*static_cast<int *>(var.m_ptr) = value.get<int>();
						break;
					case EConfigVarType::Float:
						*static_cast<float *>(var.m_ptr) = value.get<float>();
						break;
					case EConfigVarType::Color:
						*static_cast<Color_t *>(var.m_ptr) = Color_t{
							value.at(0).get<unsigned char>(),
							value.at(1).get<unsigned char>(),
							value.at(2).get<unsigned char>(),
							value.at(3).get<unsigned char>()
						};
						break;
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
