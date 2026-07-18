#pragma once

#include "../../TF2/c_tf_player.h"

#include <array>
#include <unordered_set>

enum class EEntGroup
{
	PLAYERS_ALL,
	PLAYERS_ENEMIES,
	PLAYERS_TEAMMATES,
	PLAYERS_OBSERVER,

	BUILDINGS_ALL,
	BUILDINGS_ENEMIES,
	BUILDINGS_TEAMMATES,

	PROJECTILES_ALL,
	PROJECTILES_ENEMIES,
	PROJECTILES_TEAMMATES,
	PROJECTILES_LOCAL_STICKIES,

	HEALTHPACKS,
	AMMOPACKS,
	HALLOWEEN_GIFT,
	MVM_MONEY,
	COUNT
};

class CEntityHelper
{
public:
	C_TFPlayer* GetLocal();
	C_TFWeaponBase* GetWeapon();

private:
	std::array<std::vector<C_BaseEntity*>, static_cast<size_t>(EEntGroup::COUNT)> m_arrGroups = {};
	std::unordered_set<int> m_setHealthPacks = {};
	std::unordered_set<int> m_setAmmoPacks = {};
	bool m_bGroupsReserved = false;

	bool IsHealthPack(C_BaseEntity* pEntity)
	{
		return m_setHealthPacks.contains(pEntity->m_nModelIndex());
	}

	bool IsAmmoPack(C_BaseEntity* pEntity)
	{
		return m_setAmmoPacks.contains(pEntity->m_nModelIndex());
	}

public:
	void UpdateCache();
	void UpdateModelIndexes();
	void ClearCache();

	void ClearModelIndexes()
	{
		m_setHealthPacks.clear();
		m_setAmmoPacks.clear();
	}

	const std::vector<C_BaseEntity*>& GetGroup(const EEntGroup group) { return m_arrGroups[static_cast<size_t>(group)]; }
};

MAKE_SINGLETON_SCOPED(CEntityHelper, Entities, H);
