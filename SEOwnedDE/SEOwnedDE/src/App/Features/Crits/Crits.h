#pragma once

#include "../../../SDK/SDK.h"

class CCrits
{
private:
	enum class ECritRequest
	{
		Any,
		Crit,
		Skip
	};

	int m_iCritDamage = 0;
	int m_iRangedDamage = 0;
	int m_iMeleeDamage = 0;
	int m_iResourceDamage = 0;
	int m_iDesyncDamage = 0;

	bool m_bCritBanned = false;
	float m_flDamageTilFlip = 0.0f;

	float m_flDamage = 0.0f;
	float m_flCost = 0.0f;
	int m_iAvailableCrits = 0;
	int m_iPotentialCrits = 0;
	int m_iNextCrit = 0;

	int m_iEntIndex = 0;
	bool m_bMelee = false;
	float m_flCritChance = 0.0f;
	float m_flMultCritChance = 1.0f;

	int CommandToSeed(int nCommandNumber) const;
	void UpdateWeaponInfo(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	void UpdateInfo(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon);
	ECritRequest GetCritRequest(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon);
	bool WeaponCanCrit(C_TFWeaponBase* pWeapon, bool bWeaponOnly = false) const;

public:
	void Run(CUserCmd* pCmd);
	void Paint();
	void Event(IGameEvent* pEvent, std::uint32_t eventHash);
	void Reset();
};

MAKE_SINGLETON_SCOPED(CCrits, Crits, F);
