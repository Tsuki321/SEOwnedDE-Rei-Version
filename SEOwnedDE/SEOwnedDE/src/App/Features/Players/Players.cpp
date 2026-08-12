#include "Players.h"

namespace
{
	constexpr const char* kSoftLegitKey = "softlegit";
	constexpr const char* kLegacySoftLegitKey = "retardlegit";

	bool ReadBool(const nlohmann::json& entry, const char* key, bool fallback = false)
	{
		if (!entry.is_object())
			return fallback;

		const auto it = entry.find(key);
		return it != entry.end() && it->is_boolean() ? it->get<bool>() : fallback;
	}
}

void CPlayers::Parse()
{
	// Init player data file path
	if (m_LogPath.empty())
	{
		m_LogPath = U::Storage->GetWorkFolder() / "players.json";

		if (!exists(m_LogPath))
		{
			std::ofstream file(m_LogPath, std::ios::app);

			if (!file.is_open())
			{
				return;
			}

			file.close();
		}
	}

	if (!m_Players.empty())
	{
		return;
	}

	// Open the file
	std::ifstream logFile(m_LogPath);
	if (!logFile.is_open() || logFile.peek() == std::ifstream::traits_type::eof())
	{
		return;
	}

	nlohmann::json j{};
	try
	{
		logFile >> j;
	}
	catch (const nlohmann::json::exception&)
	{
		return;
	}

	if (!j.is_object())
		return;

	bool migratedLegacyKey = false;
	for (auto& item : j.items())
	{
		const auto key = HASH_RT(item.key().c_str());
		auto& playerEntry = item.value();
		if (!playerEntry.is_object())
			continue;

		const auto softLegitIt = playerEntry.find(kSoftLegitKey);
		const bool hasValidSoftLegit = softLegitIt != playerEntry.end() && softLegitIt->is_boolean();
		const bool softLegit = ReadBool(playerEntry, kSoftLegitKey,
			ReadBool(playerEntry, kLegacySoftLegitKey));

		m_Players[key] = {
			ReadBool(playerEntry, "ignored"),
			ReadBool(playerEntry, "cheater"),
			softLegit
		};

		if (playerEntry.erase(kLegacySoftLegitKey) > 0)
		{
			if (!hasValidSoftLegit)
				playerEntry[kSoftLegitKey] = softLegit;

			migratedLegacyKey = true;
		}
	}

	logFile.close();
	if (migratedLegacyKey)
	{
		std::ofstream migratedFile(m_LogPath);
		if (migratedFile.is_open())
			migratedFile << std::setw(4) << j;
	}
}

void CPlayers::Mark(int entindex, const PlayerPriority& info)
{
	if (entindex == I::EngineClient->GetLocalPlayer())
	{
		return;
	}

	player_info_t playerInfo{};
	if (!I::EngineClient->GetPlayerInfo(entindex, &playerInfo) || playerInfo.fakeplayer)
	{
		return;
	}

	auto steamID = HASH_RT(std::string_view(playerInfo.guid).data());
	m_Players[steamID] = info;

	// Load the current playerlist
	nlohmann::json j = nlohmann::json::object();
	std::ifstream readFile(m_LogPath);
	if (readFile.is_open() && readFile.peek() != std::ifstream::traits_type::eof())
	{
		try
		{
			nlohmann::json stored{};
			readFile >> stored;
			if (stored.is_object())
				j = stored;
		}
		catch (const nlohmann::json::exception&)
		{
			j = nlohmann::json::object();
		}
	}

	readFile.close();

	// Open the output file
	std::ofstream outFile(m_LogPath);
	if (!outFile.is_open())
	{
		return;
	}

	auto& playerEntry = j[playerInfo.guid];
	if (!playerEntry.is_object())
		playerEntry = nlohmann::json::object();

	playerEntry["ignored"] = info.Ignored;
	playerEntry["cheater"] = info.Cheater;
	playerEntry[kSoftLegitKey] = info.SoftLegit;
	playerEntry.erase(kLegacySoftLegitKey);

	if (!info.Ignored && !info.Cheater && !info.SoftLegit)
	{
		j.erase(std::string(playerInfo.guid));
	}

	outFile << std::setw(4) << j;
	outFile.close();
}

bool CPlayers::GetInfo(int entindex, PlayerPriority& out)
{
	if (entindex == I::EngineClient->GetLocalPlayer())
	{
		return false;
	}

	player_info_t playerInfo{};
	if (!I::EngineClient->GetPlayerInfo(entindex, &playerInfo) || playerInfo.fakeplayer)
	{
		return false;
	}

	return GetInfoGUID(playerInfo.guid, out);
}

bool CPlayers::GetInfoGUID(const std::string& guid, PlayerPriority& out)
{
	const auto steamID = HASH_RT(guid.c_str());

	if (auto it = m_Players.find(steamID); it != std::end(m_Players))
	{
		const auto& [key, value]{ *it };
		out = value;
		return true;
	}

	return false;
}
