#include "Entities.h"
#include "../../TF2/icliententitylist.h"
#include "../../TF2/ivmodelinfo.h"
#include "../../TF2/c_baseentity.h"

C_TFPlayer* CEntityHelper::GetLocal()
{
	if (const auto pEntity = I::ClientEntityList->GetClientEntity(I::EngineClient->GetLocalPlayer()))
		return pEntity->As<C_TFPlayer>();

	return nullptr;
}

C_TFWeaponBase* CEntityHelper::GetWeapon()
{
	if (const auto pLocal = GetLocal())
	{
		if (const auto pEntity = pLocal->m_hActiveWeapon().Get())
			return pEntity->As<C_TFWeaponBase>();
	}

	return nullptr;
}

void CEntityHelper::UpdateCache()
{
	if (const auto pLocal = GetLocal())
	{
		int nLocalTeam = 0;

		if (!pLocal->IsInValidTeam(&nLocalTeam))
			return;

		// Preserve capacity across ClearCache/UpdateCache.  Reserve only once;
		// checking and repeating this block on every frame is unnecessary work.
		if (!m_bGroupsReserved)
		{
			m_arrGroups[static_cast<size_t>(EEntGroup::PLAYERS_ALL)].reserve(64);
			m_arrGroups[static_cast<size_t>(EEntGroup::PLAYERS_ENEMIES)].reserve(32);
			m_arrGroups[static_cast<size_t>(EEntGroup::PLAYERS_TEAMMATES)].reserve(32);
			m_arrGroups[static_cast<size_t>(EEntGroup::PLAYERS_OBSERVER)].reserve(16);
			m_arrGroups[static_cast<size_t>(EEntGroup::BUILDINGS_ALL)].reserve(64);
			m_arrGroups[static_cast<size_t>(EEntGroup::BUILDINGS_ENEMIES)].reserve(48);
			m_arrGroups[static_cast<size_t>(EEntGroup::BUILDINGS_TEAMMATES)].reserve(48);
			m_arrGroups[static_cast<size_t>(EEntGroup::PROJECTILES_ALL)].reserve(128);
			m_arrGroups[static_cast<size_t>(EEntGroup::PROJECTILES_ENEMIES)].reserve(96);
			m_arrGroups[static_cast<size_t>(EEntGroup::PROJECTILES_TEAMMATES)].reserve(96);
			m_arrGroups[static_cast<size_t>(EEntGroup::PROJECTILES_LOCAL_STICKIES)].reserve(32);
			m_arrGroups[static_cast<size_t>(EEntGroup::HEALTHPACKS)].reserve(32);
			m_arrGroups[static_cast<size_t>(EEntGroup::AMMOPACKS)].reserve(32);
			m_arrGroups[static_cast<size_t>(EEntGroup::HALLOWEEN_GIFT)].reserve(8);
			m_arrGroups[static_cast<size_t>(EEntGroup::MVM_MONEY)].reserve(64);
			m_bGroupsReserved = true;
		}

		const int highestEntityIndex = I::ClientEntityList->GetHighestEntityIndex();
		for (int n = 1; n <= highestEntityIndex; n++)
		{
			IClientEntity* pClientEntity = I::ClientEntityList->GetClientEntity(n);

			if (!pClientEntity || pClientEntity->IsDormant())
				continue;

			auto pEntity = pClientEntity->As<C_BaseEntity>();

			switch (pEntity->GetClassId())
			{
			case ETFClassIds::CTFPlayer:
				{
					int nPlayerTeam = 0;

					const auto pPlayer = pEntity->As<C_TFPlayer>();
					if (pPlayer->deadflag() && pPlayer->m_iObserverMode() != OBS_MODE_NONE)
					{
						m_arrGroups[static_cast<size_t>(EEntGroup::PLAYERS_OBSERVER)].push_back(pEntity);
					}

					if (!pEntity->IsInValidTeam(&nPlayerTeam))
						continue;

					m_arrGroups[static_cast<size_t>(EEntGroup::PLAYERS_ALL)].push_back(pEntity);
					m_arrGroups[static_cast<size_t>(nLocalTeam != nPlayerTeam ? EEntGroup::PLAYERS_ENEMIES : EEntGroup::PLAYERS_TEAMMATES)].push_back(pEntity);

					break;
				}

			case ETFClassIds::CObjectSentrygun:
			case ETFClassIds::CObjectDispenser:
			case ETFClassIds::CObjectTeleporter:
				{
					int nObjectTeam = 0;

					if (!pEntity->IsInValidTeam(&nObjectTeam))
						continue;

					m_arrGroups[static_cast<size_t>(EEntGroup::BUILDINGS_ALL)].push_back(pEntity);
					m_arrGroups[static_cast<size_t>(nLocalTeam != nObjectTeam ? EEntGroup::BUILDINGS_ENEMIES : EEntGroup::BUILDINGS_TEAMMATES)].push_back(pEntity);

					break;
				}

			case ETFClassIds::CTFProjectile_Rocket:
			case ETFClassIds::CTFProjectile_SentryRocket:
			case ETFClassIds::CTFProjectile_Jar:
			case ETFClassIds::CTFProjectile_JarGas:
			case ETFClassIds::CTFProjectile_JarMilk:
			case ETFClassIds::CTFProjectile_Arrow:
			case ETFClassIds::CTFProjectile_Flare:
			case ETFClassIds::CTFProjectile_Cleaver:
			case ETFClassIds::CTFProjectile_HealingBolt:
			case ETFClassIds::CTFGrenadePipebombProjectile:
			case ETFClassIds::CTFProjectile_BallOfFire:
			case ETFClassIds::CTFProjectile_EnergyRing:
			case ETFClassIds::CTFProjectile_EnergyBall:
				{
					int nProjectileTeam = 0;

					if (!pEntity->IsInValidTeam(&nProjectileTeam))
						continue;

					if (pEntity->GetClassId() == ETFClassIds::CTFGrenadePipebombProjectile)
					{
						const auto pPipebomb = pEntity->As<C_TFGrenadePipebombProjectile>();

						/*if (pPipebomb->m_iType() == TF_GL_MODE_REMOTE_DETONATE_PRACTICE)
							continue;*/

						if (pPipebomb->HasStickyEffects() && pPipebomb->As<C_BaseGrenade>()->m_hThrower().Get() == pLocal)
							m_arrGroups[static_cast<size_t>(EEntGroup::PROJECTILES_LOCAL_STICKIES)].push_back(pEntity);
					}

					m_arrGroups[static_cast<size_t>(EEntGroup::PROJECTILES_ALL)].push_back(pEntity);
					m_arrGroups[static_cast<size_t>(nLocalTeam != nProjectileTeam ? EEntGroup::PROJECTILES_ENEMIES : EEntGroup::PROJECTILES_TEAMMATES)].push_back(pEntity);

					break;
				}

			case ETFClassIds::CBaseAnimating:
				{
					if (IsHealthPack(pEntity))
						m_arrGroups[static_cast<size_t>(EEntGroup::HEALTHPACKS)].push_back(pEntity);

					if (IsAmmoPack(pEntity))
						m_arrGroups[static_cast<size_t>(EEntGroup::AMMOPACKS)].push_back(pEntity);

					break;
				}

			case ETFClassIds::CTFAmmoPack:
				{
					m_arrGroups[static_cast<size_t>(EEntGroup::AMMOPACKS)].push_back(pEntity);
					break;
				}

			case ETFClassIds::CHalloweenGiftPickup:
				{
					m_arrGroups[static_cast<size_t>(EEntGroup::HALLOWEEN_GIFT)].push_back(pEntity);

					break;
				}

			case ETFClassIds::CCurrencyPack:
				{
					if (pEntity->As<C_CurrencyPack>()->m_bDistributed())
					{
						continue;
					}

					m_arrGroups[static_cast<size_t>(EEntGroup::MVM_MONEY)].push_back(pEntity);

					break;
				}

			default: break;
			}
		}
	}
}

void CEntityHelper::UpdateModelIndexes()
{
	m_setHealthPacks.clear();
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/medkit_small.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/medkit_medium.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/medkit_large.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/props_halloween/halloween_medkit_small.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/props_halloween/halloween_medkit_medium.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/props_halloween/halloween_medkit_large.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/medkit_small_bday.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/medkit_medium_bday.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/medkit_large_bday.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/props_medieval/medieval_meat.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/plate.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/plate_sandwich_xmas.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/plate_robo_sandwich.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/workshop/weapons/c_models/c_fishcake/plate_fishcake.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/workshop/weapons/c_models/c_buffalo_steak/plate_buffalo_steak.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/workshop/weapons/c_models/c_chocolate/plate_chocolate.mdl"));
	m_setHealthPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/banana/plate_banana.mdl"));

	m_setAmmoPacks.clear();
	m_setAmmoPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/ammopack_small.mdl"));
	m_setAmmoPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/ammopack_medium.mdl"));
	m_setAmmoPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/ammopack_large.mdl"));
	m_setAmmoPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/ammopack_small_bday.mdl"));
	m_setAmmoPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/ammopack_medium_bday.mdl"));
	m_setAmmoPacks.insert(I::ModelInfoClient->GetModelIndex("models/items/ammopack_large_bday.mdl"));
}

void CEntityHelper::ClearCache()
{
	// clear() keeps capacity so the next UpdateCache does not reallocate
	// group vectors after they have grown to a stable size.
	for (auto& group : m_arrGroups)
	{
		group.clear();
	}
}
