#include "SkinChanger.h"

#include "../../../Utils/Storage/Storage.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <format>
#include <limits>

MAKE_SIGNATURE(CEconItemSystem_GetItemSchema, "client.dll", "48 83 EC 28 E8 ? ? ? ? 48 83 C0 08", 0x0);
MAKE_SIGNATURE(CEconItemSchema_GetAttributeDefinition, "client.dll", "89 54 24 ? 53 48 83 EC 20 48 8B D9 48 8D 54 24 ? 48 81 C1 50 02 00 00", 0x0);
MAKE_SIGNATURE(CAttributeList_SetRuntimeAttributeValue, "client.dll", "48 89 5C 24 ? 55 56 57 48 8B EC 48 83 EC 50 44 8B 49", 0x0);

namespace
{
	using Clock = std::chrono::steady_clock;

	constexpr auto kRuntimeRefreshDelay = std::chrono::milliseconds(250);
	constexpr auto kSaveDelay = std::chrono::milliseconds(750);

	enum class EEconAttribute : std::uint16_t
	{
		TurnVictimsToGold = 150,
		PipBoyBuildMenu = 295,
		JingleFootsteps = 364,
		UnusualEffectStatic = 370,
		StyleOverride = 542,
		HalloweenVoices = 1006,
		PumpkinBombs = 1007,
		HalloweenFlames = 1008,
		UnusualEffect = 134,
		TeamColoredPaintKit = 745,
		PaintKitWear = 725,
		AllowInspect = 731,
		PaintKit = 834,
		PaintKitSeedLow = 866,
		PaintKitSeedHigh = 867,
		KillstreakEffect = 2013,
		KillstreakSheen = 2014,
		DecoratedRarity = 2022,
		KillstreakTier = 2025,
		Australium = 2027,
		Festivized = 2053
	};

	struct RuntimeAttribute
	{
		EEconAttribute m_Attribute = {};
		float m_flValue = 0.0f;
	};

	constexpr std::size_t kMaxRuntimeAttributes = 21;

	int ParseItemDefinition(const std::string &value)
	{
		try
		{
			const auto parsed = std::stoll(value);
			if (parsed < 0 || parsed > std::numeric_limits<int>::max())
				return -1;

			return static_cast<int>(parsed);
		}
		catch (const std::exception &)
		{
			return -1;
		}
	}

	SkinChangerSettings SanitizeSettings(SkinChangerSettings settings)
	{
		settings.m_nPaintKit = std::clamp(settings.m_nPaintKit, 0, 65535);
		settings.m_flWear = std::isfinite(settings.m_flWear)
			? std::clamp(settings.m_flWear, 0.0f, 1.0f)
			: 0.0f;
		settings.m_nSeedLow = std::clamp(settings.m_nSeedLow, 0, 65535);
		settings.m_nSeedHigh = std::clamp(settings.m_nSeedHigh, 0, 65535);
		settings.m_nUnusualEffect = std::clamp(settings.m_nUnusualEffect, 0, 65535);
		settings.m_nUnusualEffectStatic = std::clamp(settings.m_nUnusualEffectStatic, 0, 65535);
		settings.m_nStyleOverride = std::clamp(settings.m_nStyleOverride, 0, 100);
		settings.m_nKillstreakTier = std::clamp(settings.m_nKillstreakTier, 0, 3);
		settings.m_nKillstreakSheen = std::clamp(settings.m_nKillstreakSheen, 0, 7);
		settings.m_nKillstreakEffect = std::clamp(settings.m_nKillstreakEffect, 0, 7);
		return settings;
	}

	nlohmann::json SerializeSettings(const SkinChangerSettings &settings)
	{
		return {
			{ "enabled", settings.m_bEnabled },
			{ "paint_kit", settings.m_nPaintKit },
			{ "wear", settings.m_flWear },
			{ "team_colored", settings.m_bTeamColored },
			{ "allow_inspect", settings.m_bAllowInspect },
			{ "seed_low", settings.m_nSeedLow },
			{ "seed_high", settings.m_nSeedHigh },
			{ "unusual_effect", settings.m_nUnusualEffect },
			{ "unusual_effect_static", settings.m_nUnusualEffectStatic },
			{ "festivized", settings.m_bFestivized },
			{ "australium", settings.m_bAustralium },
			{ "decorated_rarity", settings.m_bDecoratedRarity },
			{ "style_override", settings.m_nStyleOverride },
			{ "turn_victims_to_gold", settings.m_bTurnVictimsToGold },
			{ "killstreak_tier", settings.m_nKillstreakTier },
			{ "killstreak_sheen", settings.m_nKillstreakSheen },
			{ "killstreak_effect", settings.m_nKillstreakEffect },
			{ "pumpkin_bombs", settings.m_bPumpkinBombs },
			{ "halloween_flames", settings.m_bHalloweenFlames },
			{ "halloween_voices", settings.m_bHalloweenVoices },
			{ "jingle_footsteps", settings.m_bJingleFootsteps },
			{ "pipboy_build_menu", settings.m_bPipBoyBuildMenu }
		};
	}

	SkinChangerSettings DeserializeSettings(const nlohmann::json &value)
	{
		SkinChangerSettings settings = {};
		settings.m_bEnabled = value.value("enabled", false);
		settings.m_nPaintKit = value.value("paint_kit", 0);
		settings.m_flWear = value.value("wear", 0.0f);
		settings.m_bTeamColored = value.value("team_colored", false);
		settings.m_bAllowInspect = value.value("allow_inspect", false);
		settings.m_nSeedLow = value.value("seed_low", 0);
		settings.m_nSeedHigh = value.value("seed_high", 0);
		settings.m_nUnusualEffect = value.value("unusual_effect", 0);
		settings.m_nUnusualEffectStatic = value.value("unusual_effect_static", 0);
		settings.m_bFestivized = value.value("festivized", false);
		settings.m_bAustralium = value.value("australium", false);
		settings.m_bDecoratedRarity = value.value("decorated_rarity", false);
		settings.m_nStyleOverride = value.value("style_override", 0);
		settings.m_bTurnVictimsToGold = value.value("turn_victims_to_gold", false);
		settings.m_nKillstreakTier = value.value("killstreak_tier", 0);
		settings.m_nKillstreakSheen = value.value("killstreak_sheen", 0);
		settings.m_nKillstreakEffect = value.value("killstreak_effect", 0);
		settings.m_bPumpkinBombs = value.value("pumpkin_bombs", false);
		settings.m_bHalloweenFlames = value.value("halloween_flames", false);
		settings.m_bHalloweenVoices = value.value("halloween_voices", false);
		settings.m_bJingleFootsteps = value.value("jingle_footsteps", false);
		settings.m_bPipBoyBuildMenu = value.value("pipboy_build_menu", false);
		return SanitizeSettings(settings);
	}

	void ApplyLegacyAttribute(SkinChangerSettings &settings, int nAttribute, float flValue)
	{
		switch (static_cast<EEconAttribute>(nAttribute))
		{
			case EEconAttribute::PaintKit:
			{
				const int packedValue = std::bit_cast<int>(flValue);
				if (packedValue >= 0 && packedValue <= 65535)
					settings.m_nPaintKit = packedValue;
				else if (std::isfinite(flValue) && flValue >= 0.0f && flValue <= 65535.0f)
					settings.m_nPaintKit = static_cast<int>(flValue);
				break;
			}
			case EEconAttribute::PaintKitWear: settings.m_flWear = flValue; break;
			case EEconAttribute::TeamColoredPaintKit: settings.m_bTeamColored = flValue != 0.0f; break;
			case EEconAttribute::AllowInspect: settings.m_bAllowInspect = flValue != 0.0f; break;
			case EEconAttribute::PaintKitSeedLow: settings.m_nSeedLow = static_cast<int>(flValue); break;
			case EEconAttribute::PaintKitSeedHigh: settings.m_nSeedHigh = static_cast<int>(flValue); break;
			case EEconAttribute::UnusualEffect: settings.m_nUnusualEffect = static_cast<int>(flValue); break;
			case EEconAttribute::UnusualEffectStatic: settings.m_nUnusualEffectStatic = static_cast<int>(flValue); break;
			case EEconAttribute::Festivized: settings.m_bFestivized = flValue != 0.0f; break;
			case EEconAttribute::Australium: settings.m_bAustralium = flValue != 0.0f; break;
			case EEconAttribute::DecoratedRarity: settings.m_bDecoratedRarity = flValue != 0.0f; break;
			case EEconAttribute::StyleOverride: settings.m_nStyleOverride = static_cast<int>(flValue); break;
			case EEconAttribute::TurnVictimsToGold: settings.m_bTurnVictimsToGold = flValue != 0.0f; break;
			case EEconAttribute::KillstreakTier: settings.m_nKillstreakTier = static_cast<int>(flValue); break;
			case EEconAttribute::KillstreakSheen: settings.m_nKillstreakSheen = static_cast<int>(flValue); break;
			case EEconAttribute::KillstreakEffect: settings.m_nKillstreakEffect = static_cast<int>(flValue); break;
			case EEconAttribute::PumpkinBombs: settings.m_bPumpkinBombs = flValue != 0.0f; break;
			case EEconAttribute::HalloweenFlames: settings.m_bHalloweenFlames = flValue != 0.0f; break;
			case EEconAttribute::HalloweenVoices: settings.m_bHalloweenVoices = flValue != 0.0f; break;
			case EEconAttribute::JingleFootsteps: settings.m_bJingleFootsteps = flValue != 0.0f; break;
			case EEconAttribute::PipBoyBuildMenu: settings.m_bPipBoyBuildMenu = flValue != 0.0f; break;
			default: break;
		}
	}

	const char *GetFriendlyWeaponName(int nItemDefinition)
	{
		switch (nItemDefinition)
		{
			case Scout_m_ScattergunR: return "Scattergun";
			case Soldier_m_RocketLauncherR: return "Rocket Launcher";
			case Pyro_m_FlameThrowerR: return "Flame Thrower";
			case Demoman_m_GrenadeLauncherR: return "Grenade Launcher";
			case Demoman_s_StickybombLauncherR: return "Stickybomb Launcher";
			case Heavy_m_MinigunR: return "Minigun";
			case Engi_t_WrenchR: return "Wrench";
			case Medic_s_MediGunR: return "Medi Gun";
			case Sniper_m_SniperRifleR: return "Sniper Rifle";
			case Sniper_s_SMGR: return "SMG";
			case Spy_t_KnifeR: return "Knife";
			case Spy_m_RevolverR: return "Revolver";
			case Engi_s_PistolR: return "Pistol";
			case Soldier_s_ShotgunR: return "Shotgun";
			case Scout_t_BatR: return "Bat";
			case Soldier_t_ShovelR: return "Shovel";
			case Pyro_t_FireAxeR: return "Fire Axe";
			case Demoman_t_BottleR: return "Bottle";
			case Medic_t_BonesawR: return "Bonesaw";
			case Sniper_t_KukriR: return "Kukri";
			default: return nullptr;
		}
	}
}

CSkinChanger::CSkinChanger()
{
	m_mapProfiles.reserve(MAX_WEAPONS);
	m_mapAttributeDefinitions.reserve(kMaxRuntimeAttributes);
	ResetAppliedWeapons();
}

void CSkinChanger::ResetAppliedWeapons()
{
	for (auto &state : m_arrAppliedWeapons)
		state = {};
}

void CSkinChanger::ScheduleRuntimeRefresh()
{
	m_bRuntimeRefreshPending = true;
	m_RuntimeRefreshDeadline = Clock::now() + kRuntimeRefreshDelay;
}

void CSkinChanger::ScheduleSave()
{
	m_bSavePending = true;
	m_SaveDeadline = Clock::now() + kSaveDelay;
}

void CSkinChanger::FlushScheduledSave()
{
	if (m_bSavePending && Clock::now() >= m_SaveDeadline)
		Save();
}

bool CSkinChanger::HasEnabledProfiles() const
{
	for (const auto &entry : m_mapProfiles)
	{
		if (entry.second.m_Settings.m_bEnabled)
			return true;
	}

	return false;
}

void *CSkinChanger::GetAttributeDefinition(std::uint16_t nAttributeIndex)
{
	if (const auto found = m_mapAttributeDefinitions.find(nAttributeIndex); found != m_mapAttributeDefinitions.end())
		return found->second;

	using GetItemSchemaFn = void *(__fastcall *)();
	using GetAttributeDefinitionFn = void *(__fastcall *)(void *, int);
	const auto dwGetItemSchema = Signatures::CEconItemSystem_GetItemSchema.Get();
	const auto dwGetAttributeDefinition = Signatures::CEconItemSchema_GetAttributeDefinition.Get();
	if (!dwGetItemSchema || !dwGetAttributeDefinition)
		return nullptr;

	if (!m_pItemSchema)
		m_pItemSchema = reinterpret_cast<GetItemSchemaFn>(dwGetItemSchema)();

	void *pDefinition = nullptr;
	if (m_pItemSchema)
	{
		pDefinition = reinterpret_cast<GetAttributeDefinitionFn>(dwGetAttributeDefinition)(
			m_pItemSchema,
			static_cast<int>(nAttributeIndex)
		);
	}

	if (pDefinition)
		m_mapAttributeDefinitions.emplace(nAttributeIndex, pDefinition);
	return pDefinition;
}

bool CSkinChanger::SetRuntimeAttribute(void *pAttributeList, std::uint16_t nAttributeIndex, float flValue)
{
	const auto dwSetRuntimeAttributeValue = Signatures::CAttributeList_SetRuntimeAttributeValue.Get();
	if (!pAttributeList || !dwSetRuntimeAttributeValue)
		return false;

	const auto pDefinition = GetAttributeDefinition(nAttributeIndex);
	if (!pDefinition)
		return false;

	using SetRuntimeAttributeValueFn = void(__fastcall *)(void *, void *, float);
	reinterpret_cast<SetRuntimeAttributeValueFn>(dwSetRuntimeAttributeValue)(
		pAttributeList,
		pDefinition,
		flValue
	);
	return true;
}

CSkinChanger::ApplyResult CSkinChanger::ApplyProfile(C_TFWeaponBase *pWeapon, int nItemDefinition, const Profile &profile)
{
	if (!pWeapon || !profile.m_Settings.m_bEnabled)
		return ApplyResult::PermanentFailure;

	const auto dwSetRuntimeAttributeValue = Signatures::CAttributeList_SetRuntimeAttributeValue.Get();
	static int nAttributeListOffset = 0;
	if (nAttributeListOffset <= 0)
		nAttributeListOffset = NetVars::GetNetVar("CEconEntity", "m_AttributeList");
	if (nAttributeListOffset <= 0 || !dwSetRuntimeAttributeValue)
		return ApplyResult::TransientFailure;

	int &nWeaponItemDefinition = pWeapon->m_iItemDefinitionIndex();
	nWeaponItemDefinition = nItemDefinition;

	auto pAttributeList = reinterpret_cast<void *>(
		reinterpret_cast<std::uintptr_t>(pWeapon) + static_cast<std::uintptr_t>(nAttributeListOffset)
	);

	const auto &settings = profile.m_Settings;
	std::array<RuntimeAttribute, kMaxRuntimeAttributes> attributes = {};
	std::size_t nAttributeCount = 0;

	const auto addAttribute = [&](EEconAttribute attribute, float flValue)
	{
		if (nAttributeCount < attributes.size())
			attributes[nAttributeCount++] = { attribute, flValue };
	};

	if (settings.m_nPaintKit > 0)
	{
		addAttribute(EEconAttribute::PaintKit, std::bit_cast<float>(settings.m_nPaintKit));
		addAttribute(EEconAttribute::PaintKitWear, settings.m_flWear);
	}
	if (settings.m_nSeedLow > 0)
		addAttribute(EEconAttribute::PaintKitSeedLow, static_cast<float>(settings.m_nSeedLow));
	if (settings.m_nSeedHigh > 0)
		addAttribute(EEconAttribute::PaintKitSeedHigh, static_cast<float>(settings.m_nSeedHigh));
	if (settings.m_bTeamColored)
		addAttribute(EEconAttribute::TeamColoredPaintKit, 1.0f);
	if (settings.m_bAllowInspect)
		addAttribute(EEconAttribute::AllowInspect, 1.0f);
	if (settings.m_nUnusualEffect > 0)
		addAttribute(EEconAttribute::UnusualEffect, static_cast<float>(settings.m_nUnusualEffect));
	if (settings.m_nUnusualEffectStatic > 0)
		addAttribute(EEconAttribute::UnusualEffectStatic, static_cast<float>(settings.m_nUnusualEffectStatic));
	if (settings.m_bFestivized)
		addAttribute(EEconAttribute::Festivized, 1.0f);
	if (settings.m_bAustralium)
		addAttribute(EEconAttribute::Australium, 1.0f);
	if (settings.m_bDecoratedRarity)
		addAttribute(EEconAttribute::DecoratedRarity, 1.0f);
	if (settings.m_nStyleOverride > 0)
		addAttribute(EEconAttribute::StyleOverride, static_cast<float>(settings.m_nStyleOverride));
	if (settings.m_bTurnVictimsToGold)
		addAttribute(EEconAttribute::TurnVictimsToGold, 1.0f);
	if (settings.m_nKillstreakTier > 0)
		addAttribute(EEconAttribute::KillstreakTier, static_cast<float>(settings.m_nKillstreakTier));
	if (settings.m_nKillstreakSheen > 0)
		addAttribute(EEconAttribute::KillstreakSheen, static_cast<float>(settings.m_nKillstreakSheen));
	if (settings.m_nKillstreakEffect > 0)
		addAttribute(EEconAttribute::KillstreakEffect, static_cast<float>(settings.m_nKillstreakEffect));
	if (settings.m_bPumpkinBombs)
		addAttribute(EEconAttribute::PumpkinBombs, 1.0f);
	if (settings.m_bHalloweenFlames)
		addAttribute(EEconAttribute::HalloweenFlames, 1.0f);
	if (settings.m_bHalloweenVoices)
		addAttribute(EEconAttribute::HalloweenVoices, 1.0f);
	if (settings.m_bJingleFootsteps)
		addAttribute(EEconAttribute::JingleFootsteps, 1.0f);
	if (settings.m_bPipBoyBuildMenu)
		addAttribute(EEconAttribute::PipBoyBuildMenu, 1.0f);

	if (nAttributeCount == 0)
		return ApplyResult::Success;

	const auto dwGetItemSchema = Signatures::CEconItemSystem_GetItemSchema.Get();
	const auto dwGetAttributeDefinition = Signatures::CEconItemSchema_GetAttributeDefinition.Get();
	if (!dwGetItemSchema || !dwGetAttributeDefinition)
		return ApplyResult::TransientFailure;

	if (!m_pItemSchema)
		m_pItemSchema = reinterpret_cast<void *(__fastcall *)()>(dwGetItemSchema)();
	if (!m_pItemSchema)
		return ApplyResult::TransientFailure;

	bool bAppliedAllAttributes = true;
	for (std::size_t i = 0; i < nAttributeCount; ++i)
	{
		bAppliedAllAttributes &= SetRuntimeAttribute(
			pAttributeList,
			static_cast<std::uint16_t>(attributes[i].m_Attribute),
			attributes[i].m_flValue
		);
	}

	if (!bAppliedAllAttributes)
		return ApplyResult::PermanentFailure;

	return ApplyResult::Success;
}

void CSkinChanger::Run()
{
	FlushScheduledSave();
	m_nCurrentWeaponIndex = -1;

	if (!I::EngineClient || !I::ClientEntityList || !I::ClientState || !I::EngineClient->IsInGame())
		return;

	const int nLocalPlayer = I::EngineClient->GetLocalPlayer();
	if (nLocalPlayer <= 0)
		return;

	const auto pLocalEntity = I::ClientEntityList->GetClientEntity(nLocalPlayer);
	if (!pLocalEntity)
		return;

	const auto pLocal = pLocalEntity->As<C_TFPlayer>();
	if (const auto pActiveWeapon = reinterpret_cast<C_TFWeaponBase *>(pLocal->m_hActiveWeapon().Get()))
		m_nCurrentWeaponIndex = NormalizeItemDefinition(pActiveWeapon->m_iItemDefinitionIndex());

	if (m_bRuntimeRefreshPending)
	{
		if (Clock::now() < m_RuntimeRefreshDeadline)
			return;

		I::ClientState->m_nDeltaTick = -1;
		m_bRuntimeRefreshPending = false;
		m_bAwaitingFullUpdate = true;
		ResetAppliedWeapons();
		return;
	}

	if (m_bAwaitingFullUpdate)
	{
		if (I::ClientState->m_nDeltaTick <= 0)
			return;

		m_bAwaitingFullUpdate = false;
		ResetAppliedWeapons();
	}

	if (!HasEnabledProfiles())
		return;

	static int nWeaponsOffset = 0;
	if (nWeaponsOffset <= 0)
		nWeaponsOffset = NetVars::GetNetVar("CBaseCombatCharacter", "m_hMyWeapons");
	if (nWeaponsOffset <= 0)
		return;

	const auto pWeaponHandles = reinterpret_cast<const EHANDLE *>(
		reinterpret_cast<std::uintptr_t>(pLocal) + static_cast<std::uintptr_t>(nWeaponsOffset)
	);

	for (int i = 0; i < MAX_WEAPONS; ++i)
	{
		const auto &handle = pWeaponHandles[i];
		if (!handle.IsValid())
			continue;

		const auto pWeapon = reinterpret_cast<C_TFWeaponBase *>(handle.Get());
		if (!pWeapon)
			continue;

		const int nItemDefinition = NormalizeItemDefinition(pWeapon->m_iItemDefinitionIndex());
		const auto profile = m_mapProfiles.find(nItemDefinition);
		if (profile == m_mapProfiles.end() || !profile->second.m_Settings.m_bEnabled)
			continue;

		const int nEntryIndex = handle.GetEntryIndex();
		if (nEntryIndex < 0 || nEntryIndex >= static_cast<int>(m_arrAppliedWeapons.size()))
			continue;

		auto &applied = m_arrAppliedWeapons[nEntryIndex];
		if (applied.m_nHandle == handle.ToInt()
			&& applied.m_nItemDefinition == nItemDefinition
			&& applied.m_nRevision == profile->second.m_nRevision)
		{
			continue;
		}

		const auto result = ApplyProfile(pWeapon, nItemDefinition, profile->second);
		if (result == ApplyResult::TransientFailure)
			continue;

		applied = { handle.ToInt(), nItemDefinition, profile->second.m_nRevision };
	}
}

void CSkinChanger::ResetRuntimeState()
{
	if (m_bSavePending)
		Save();

	m_nCurrentWeaponIndex = -1;
	m_bRuntimeRefreshPending = false;
	m_bAwaitingFullUpdate = false;
	m_pItemSchema = nullptr;
	m_mapAttributeDefinitions.clear();
	ResetAppliedWeapons();
}

SkinChangerSettings CSkinChanger::GetSettings(int nItemDefinition) const
{
	nItemDefinition = NormalizeItemDefinition(nItemDefinition);
	if (const auto found = m_mapProfiles.find(nItemDefinition); found != m_mapProfiles.end())
		return found->second.m_Settings;

	return {};
}

void CSkinChanger::SetSettings(int nItemDefinition, const SkinChangerSettings &settings)
{
	nItemDefinition = NormalizeItemDefinition(nItemDefinition);
	if (nItemDefinition < 0)
		return;

	const auto sanitized = SanitizeSettings(settings);
	const auto previous = GetSettings(nItemDefinition);
	if (sanitized == previous)
		return;

	if (sanitized.IsDefault())
	{
		m_mapProfiles.erase(nItemDefinition);
	}
	else
	{
		m_mapProfiles[nItemDefinition] = { sanitized, m_nNextRevision++ };
	}

	if (previous.m_bEnabled || sanitized.m_bEnabled)
		ScheduleRuntimeRefresh();

	ScheduleSave();
}

void CSkinChanger::RemoveSettings(int nItemDefinition)
{
	SetSettings(nItemDefinition, {});
}

bool CSkinChanger::Load()
{
	if (U::Storage->GetWorkFolder().empty())
		return false;

	const auto storagePath = U::Storage->GetWorkFolder() / "skins.json";
	auto sourcePath = storagePath;
	bool bLegacyPath = false;

	if (!std::filesystem::exists(sourcePath))
	{
		const auto legacyPath = std::filesystem::current_path() / "skins.json";
		if (!std::filesystem::exists(legacyPath))
			return false;

		sourcePath = legacyPath;
		bLegacyPath = true;
	}

	std::ifstream input(sourcePath);
	if (!input.is_open())
		return false;

	nlohmann::json root = {};
	try
	{
		input >> root;
	}
	catch (const nlohmann::json::exception &)
	{
		return false;
	}

	std::unordered_map<int, Profile> loadedProfiles = {};
	loadedProfiles.reserve(MAX_WEAPONS);

	try
	{
		if (const auto weapons = root.find("weapons"); weapons != root.end() && weapons->is_object())
		{
			for (auto it = weapons->begin(); it != weapons->end(); ++it)
			{
				const int nItemDefinition = NormalizeItemDefinition(ParseItemDefinition(it.key()));
				if (nItemDefinition < 0 || !it.value().is_object())
					continue;

				const auto settings = DeserializeSettings(it.value());
				if (!settings.IsDefault())
					loadedProfiles[nItemDefinition] = { settings, m_nNextRevision++ };
			}
		}
		else if (root.is_object())
		{
			bLegacyPath = true;
			for (auto it = root.begin(); it != root.end(); ++it)
			{
				const int nItemDefinition = NormalizeItemDefinition(ParseItemDefinition(it.key()));
				if (nItemDefinition < 0 || !it.value().is_object())
					continue;

				SkinChangerSettings settings = {};
				settings.m_bEnabled = true;
				for (auto attribute = it.value().begin(); attribute != it.value().end(); ++attribute)
				{
					const int nAttribute = ParseItemDefinition(attribute.key());
					if (nAttribute >= 0 && attribute.value().is_number())
						ApplyLegacyAttribute(settings, nAttribute, attribute.value().get<float>());
				}

				settings = SanitizeSettings(settings);
				loadedProfiles[nItemDefinition] = { settings, m_nNextRevision++ };
			}
		}
		else
		{
			return false;
		}
	}
	catch (const nlohmann::json::exception &)
	{
		return false;
	}

	m_mapProfiles.swap(loadedProfiles);
	ResetAppliedWeapons();
	if (HasEnabledProfiles())
		ScheduleRuntimeRefresh();
	m_bSavePending = false;

	if (bLegacyPath)
		Save();

	return true;
}

bool CSkinChanger::Save()
{
	if (U::Storage->GetWorkFolder().empty())
		return false;

	const auto path = U::Storage->GetWorkFolder() / "skins.json";
	std::ofstream output(path);
	if (!output.is_open())
		return false;

	nlohmann::json root = {
		{ "version", 1 },
		{ "weapons", nlohmann::json::object() }
	};

	for (const auto &[nItemDefinition, profile] : m_mapProfiles)
	{
		if (!profile.m_Settings.IsDefault())
			root["weapons"][std::to_string(nItemDefinition)] = SerializeSettings(profile.m_Settings);
	}

	output << root.dump(4);
	if (!output.good())
		return false;

	m_bSavePending = false;
	return true;
}

std::string CSkinChanger::GetCurrentWeaponLabel() const
{
	if (m_nCurrentWeaponIndex < 0)
		return "No active weapon";

	if (const auto friendlyName = GetFriendlyWeaponName(m_nCurrentWeaponIndex))
		return std::format("{} [{}]", friendlyName, m_nCurrentWeaponIndex);

	return std::format("Item [{}]", m_nCurrentWeaponIndex);
}

int CSkinChanger::NormalizeItemDefinition(int nItemDefinition)
{
	switch (nItemDefinition)
	{
		case Soldier_m_RocketLauncher: return Soldier_m_RocketLauncherR;
		case Scout_m_Scattergun: return Scout_m_ScattergunR;
		case Pyro_m_FlameThrower: return Pyro_m_FlameThrowerR;
		case Demoman_m_GrenadeLauncher: return Demoman_m_GrenadeLauncherR;
		case Demoman_s_StickybombLauncher: return Demoman_s_StickybombLauncherR;
		case Heavy_m_Minigun: return Heavy_m_MinigunR;
		case Engi_t_Wrench: return Engi_t_WrenchR;
		case Medic_s_MediGun: return Medic_s_MediGunR;
		case Sniper_m_SniperRifle: return Sniper_m_SniperRifleR;
		case Sniper_s_SMG: return Sniper_s_SMGR;
		case Spy_t_Knife: return Spy_t_KnifeR;
		case Spy_m_Revolver: return Spy_m_RevolverR;
		case Engi_s_EngineersPistol: return Engi_s_PistolR;
		case Soldier_s_SoldiersShotgun:
		case Pyro_s_PyrosShotgun:
		case Heavy_s_HeavysShotgun:
		case Engi_m_EngineersShotgun:
			return Soldier_s_ShotgunR;
		case Scout_t_Bat: return Scout_t_BatR;
		case Soldier_t_Shovel: return Soldier_t_ShovelR;
		case Pyro_t_FireAxe: return Pyro_t_FireAxeR;
		case Demoman_t_Bottle: return Demoman_t_BottleR;
		case Medic_t_Bonesaw: return Medic_t_BonesawR;
		case Sniper_t_Kukri: return Sniper_t_KukriR;
		default: return nItemDefinition;
	}
}
