#pragma once

#include "../../../SDK/SDK.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

struct SkinChangerSettings
{
	bool m_bEnabled = false;
	int m_nPaintKit = 0;
	float m_flWear = 0.0f;
	bool m_bTeamColored = false;
	bool m_bAllowInspect = false;
	int m_nSeedLow = 0;
	int m_nSeedHigh = 0;

	int m_nUnusualEffect = 0;
	int m_nUnusualEffectStatic = 0;
	bool m_bFestivized = false;
	bool m_bAustralium = false;
	bool m_bDecoratedRarity = false;
	int m_nStyleOverride = 0;
	bool m_bTurnVictimsToGold = false;

	int m_nKillstreakTier = 0;
	int m_nKillstreakSheen = 0;
	int m_nKillstreakEffect = 0;
	bool m_bPumpkinBombs = false;
	bool m_bHalloweenFlames = false;
	bool m_bHalloweenVoices = false;
	bool m_bJingleFootsteps = false;
	bool m_bPipBoyBuildMenu = false;

	bool operator==(const SkinChangerSettings &) const = default;

	bool IsDefault() const
	{
		return *this == SkinChangerSettings{};
	}
};

class CSkinChanger
{
	struct Profile
	{
		SkinChangerSettings m_Settings = {};
		std::uint64_t m_nRevision = 0;
	};

	struct AppliedWeaponState
	{
		int m_nHandle = static_cast<int>(INVALID_EHANDLE_INDEX);
		int m_nItemDefinition = -1;
		std::uint64_t m_nRevision = 0;
	};

	std::unordered_map<int, Profile> m_mapProfiles = {};
	std::unordered_map<std::uint16_t, void *> m_mapAttributeDefinitions = {};
	std::array<AppliedWeaponState, NUM_ENT_ENTRIES> m_arrAppliedWeapons = {};

	void *m_pItemSchema = nullptr;
	std::uint64_t m_nNextRevision = 1;
	int m_nCurrentWeaponIndex = -1;
	bool m_bRuntimeRefreshPending = false;
	bool m_bAwaitingFullUpdate = false;
	bool m_bSavePending = false;
	std::chrono::steady_clock::time_point m_RuntimeRefreshDeadline = {};
	std::chrono::steady_clock::time_point m_SaveDeadline = {};

	void ResetAppliedWeapons();
	void ScheduleRuntimeRefresh();
	void ScheduleSave();
	void FlushScheduledSave();
	bool HasEnabledProfiles() const;
	enum class ApplyResult
	{
		Success,
		TransientFailure,
		PermanentFailure
	};

	void *GetAttributeDefinition(std::uint16_t nAttributeIndex);
	bool SetRuntimeAttribute(void *pAttributeList, std::uint16_t nAttributeIndex, float flValue);
	ApplyResult ApplyProfile(C_TFWeaponBase *pWeapon, int nItemDefinition, const Profile &profile);
	int GetAttributeListCount(C_TFWeaponBase *pWeapon);

public:
	CSkinChanger();

	void Run();
	void ResetRuntimeState();

	SkinChangerSettings GetSettings(int nItemDefinition) const;
	void SetSettings(int nItemDefinition, const SkinChangerSettings &settings);
	void RemoveSettings(int nItemDefinition);

	bool Load();
	bool Save();

	int GetCurrentWeaponIndex() const { return m_nCurrentWeaponIndex; }
	std::string GetCurrentWeaponLabel() const;

	static int NormalizeItemDefinition(int nItemDefinition);
};

MAKE_SINGLETON_SCOPED(CSkinChanger, SkinChanger, F);
