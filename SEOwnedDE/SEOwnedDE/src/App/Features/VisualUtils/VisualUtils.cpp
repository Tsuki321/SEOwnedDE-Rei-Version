#include "VisualUtils.h"

#include "Icons.h"
#include "../CFG.h"

#include "../Players/Players.h"

void CVisualUtils::ResetFrameCacheIfNeeded(const C_TFPlayer* pLocal)
{
	const int nFrame = I::GlobalVars ? I::GlobalVars->framecount : -1;

	if (m_nCachedFrame != nFrame || m_pCachedLocal != pLocal)
	{
		m_nCachedFrame = nFrame;
		m_pCachedLocal = pLocal;
		m_mapFrameCache.clear();
		m_bEntityCandidatesPrepared = false;
		m_vecPlayerCandidates.clear();
		m_vecBuildingCandidates.clear();
		m_vecProjectileCandidates.clear();
		m_bModelCandidatesPrepared = false;
		m_vecModelPlayerCandidates.clear();
		m_vecModelBuildingCandidates.clear();
		m_vecModelProjectileCandidates.clear();
	}

	if (pLocal)
	{
		m_vCachedLocalOrigin = pLocal->GetAbsOrigin();
	}

	m_nCachedScreenW = H::Draw->GetScreenW();
	m_nCachedScreenH = H::Draw->GetScreenH();
}

CVisualUtils::FrameCacheEntry& CVisualUtils::GetFrameCacheEntry(const C_BaseEntity* pEntity, const C_TFPlayer* pLocal)
{
	auto [it, inserted] = m_mapFrameCache.try_emplace(pEntity);
	auto& entry = it->second;

	if (entry.Frame != m_nCachedFrame || entry.Local != pLocal)
	{
		entry = {};
		entry.Frame = m_nCachedFrame;
		entry.Local = pLocal;
	}

	return entry;
}

bool CVisualUtils::IsOwnedByLocalCached(const C_TFPlayer* pLocal, const C_BaseEntity* pEntity)
{
	if (!pLocal || !pEntity)
		return false;

	ResetFrameCacheIfNeeded(pLocal);

	auto& cache = GetFrameCacheEntry(pEntity, pLocal);

	if (cache.OwnedByLocalValid)
		return cache.OwnedByLocal;

	cache.OwnedByLocal = pEntity == pLocal || IsEntityOwnedBy(
		const_cast<C_BaseEntity*>(pEntity),
		const_cast<C_TFPlayer*>(pLocal)
	);
	cache.OwnedByLocalValid = true;

	return cache.OwnedByLocal;
}

void CVisualUtils::BuildEntityCandidatesIfNeeded(C_TFPlayer* pLocal)
{
	ResetFrameCacheIfNeeded(pLocal);

	if (!pLocal || m_bEntityCandidatesPrepared)
		return;

	m_bEntityCandidatesPrepared = true;

	const auto& players = H::Entities->GetGroup(EEntGroup::PLAYERS_ALL);
	m_vecPlayerCandidates.reserve(players.size());

	for (const auto pEntity : players)
	{
		if (!pEntity)
			continue;

		auto pPlayer = pEntity->As<C_TFPlayer>();

		if (!pPlayer || pPlayer->deadflag())
			continue;

		m_vecPlayerCandidates.push_back(pPlayer);
	}

	const auto& buildings = H::Entities->GetGroup(EEntGroup::BUILDINGS_ALL);
	m_vecBuildingCandidates.reserve(buildings.size());

	for (const auto pEntity : buildings)
	{
		if (!pEntity)
			continue;

		auto pBuilding = pEntity->As<C_BaseObject>();

		if (!pBuilding || pBuilding->m_bPlacing())
			continue;

		m_vecBuildingCandidates.push_back(pBuilding);
	}

	const auto& projectiles = H::Entities->GetGroup(EEntGroup::PROJECTILES_ALL);
	m_vecProjectileCandidates.reserve(projectiles.size());

	for (const auto pEntity : projectiles)
	{
		if (!pEntity || !pEntity->ShouldDraw())
			continue;

		m_vecProjectileCandidates.push_back(pEntity);
	}
}

void CVisualUtils::BuildModelCandidatesIfNeeded(C_TFPlayer* pLocal)
{
	ResetFrameCacheIfNeeded(pLocal);

	if (!pLocal || m_bModelCandidatesPrepared)
		return;

	BuildEntityCandidatesIfNeeded(pLocal);

	m_bModelCandidatesPrepared = true;

	m_vecModelPlayerCandidates.reserve(m_vecPlayerCandidates.size());

	for (const auto pPlayer : m_vecPlayerCandidates)
	{
		if (!IsOnScreen(pLocal, pPlayer))
			continue;

		m_vecModelPlayerCandidates.push_back(pPlayer);
	}

	m_vecModelBuildingCandidates.reserve(m_vecBuildingCandidates.size());

	for (const auto pBuilding : m_vecBuildingCandidates)
	{
		if (!IsOnScreen(pLocal, pBuilding))
			continue;

		m_vecModelBuildingCandidates.push_back(pBuilding);
	}

	m_vecModelProjectileCandidates.reserve(m_vecProjectileCandidates.size());

	for (const auto pEntity : m_vecProjectileCandidates)
	{
		if (!IsOnScreen(pLocal, pEntity))
			continue;

		m_vecModelProjectileCandidates.push_back(pEntity);
	}
}

const std::vector<C_TFPlayer*>& CVisualUtils::GetPlayerCandidates(C_TFPlayer* pLocal)
{
	BuildEntityCandidatesIfNeeded(pLocal);
	return m_vecPlayerCandidates;
}

const std::vector<C_BaseObject*>& CVisualUtils::GetBuildingCandidates(C_TFPlayer* pLocal)
{
	BuildEntityCandidatesIfNeeded(pLocal);
	return m_vecBuildingCandidates;
}

const std::vector<C_BaseEntity*>& CVisualUtils::GetProjectileCandidates(C_TFPlayer* pLocal)
{
	BuildEntityCandidatesIfNeeded(pLocal);
	return m_vecProjectileCandidates;
}

const std::vector<C_TFPlayer*>& CVisualUtils::GetModelPlayerCandidates(C_TFPlayer* pLocal)
{
	BuildModelCandidatesIfNeeded(pLocal);
	return m_vecModelPlayerCandidates;
}

const std::vector<C_BaseObject*>& CVisualUtils::GetModelBuildingCandidates(C_TFPlayer* pLocal)
{
	BuildModelCandidatesIfNeeded(pLocal);
	return m_vecModelBuildingCandidates;
}

const std::vector<C_BaseEntity*>& CVisualUtils::GetModelProjectileCandidates(C_TFPlayer* pLocal)
{
	BuildModelCandidatesIfNeeded(pLocal);
	return m_vecModelProjectileCandidates;
}

bool CVisualUtils::IsEntityOwnedBy(C_BaseEntity* pEntity, C_BaseEntity* pWho)
{
	switch (pEntity->GetClassId())
	{
	case ETFClassIds::CTFGrenadePipebombProjectile:
	case ETFClassIds::CTFProjectile_Jar:
	case ETFClassIds::CTFProjectile_JarGas:
	case ETFClassIds::CTFProjectile_JarMilk:
	case ETFClassIds::CTFProjectile_Cleaver:
		{
			return pEntity->As<C_BaseGrenade>()->m_hThrower().Get() == pWho;
		}

	case ETFClassIds::CTFProjectile_SentryRocket:
		{
			if (const auto pOwner = pEntity->m_hOwnerEntity().Get())
				return pOwner->As<C_BaseObject>()->m_hBuilder() == pWho;

			break;
		}

	case ETFClassIds::CObjectSentrygun:
	case ETFClassIds::CObjectDispenser:
	case ETFClassIds::CObjectTeleporter:
		{
			return pEntity->As<C_BaseObject>()->m_hBuilder() == pWho;
		}

	default: return pEntity->m_hOwnerEntity().Get() == pWho;
	}

	return false;
}

bool CVisualUtils::ShouldRenderPlayer(
	const C_TFPlayer* pLocal,
	const C_TFPlayer* pPlayer,
	bool bIgnoreLocal,
	bool bIgnoreFriends,
	bool bIgnoreTeammates,
	bool bShowTeammateMedics,
	bool bIgnoreEnemies,
	bool bIgnoreInvisible)
{
	if (!pLocal || !pPlayer || pPlayer->deadflag())
		return false;

	const bool bIsLocal = pPlayer == pLocal;
	const bool bIsFriend = pPlayer->IsPlayerOnSteamFriendsList();

	if (bIgnoreLocal && bIsLocal)
		return false;

	if (bIgnoreFriends && bIsFriend)
		return false;

	if (!bIsLocal)
	{
		if (!bIsFriend)
		{
			if (bIgnoreTeammates && pPlayer->m_iTeamNum() == pLocal->m_iTeamNum())
			{
				if (bShowTeammateMedics)
				{
					if (pPlayer->m_iClass() != TF_CLASS_MEDIC)
						return false;
				}
				else
				{
					return false;
				}
			}

			if (bIgnoreEnemies && pPlayer->m_iTeamNum() != pLocal->m_iTeamNum())
				return false;
		}

		if (bIgnoreInvisible && pPlayer->m_flInvisibility() >= 1.0f)
			return false;
	}

	return true;
}

bool CVisualUtils::ShouldRenderBuilding(
	const C_TFPlayer* pLocal,
	const C_BaseObject* pBuilding,
	bool bIgnoreLocal,
	bool bIgnoreTeammates,
	bool bShowTeammateDispensers,
	bool bIgnoreEnemies)
{
	if (!pLocal || !pBuilding || pBuilding->m_bPlacing())
		return false;

	const bool bIsLocal = IsOwnedByLocalCached(pLocal, pBuilding);

	if (bIgnoreLocal && bIsLocal)
		return false;

	if (!bIsLocal)
	{
		if (bIgnoreTeammates && pBuilding->m_iTeamNum() == pLocal->m_iTeamNum())
		{
			if (bShowTeammateDispensers)
			{
				if (pBuilding->GetClassId() != ETFClassIds::CObjectDispenser)
					return false;
			}
			else
			{
				return false;
			}
		}

		if (bIgnoreEnemies && pBuilding->m_iTeamNum() != pLocal->m_iTeamNum())
			return false;
	}

	return true;
}

bool CVisualUtils::ShouldRenderProjectile(
	const C_TFPlayer* pLocal,
	const C_BaseEntity* pProjectile,
	bool bIgnoreLocal,
	bool bIgnoreEnemies,
	bool bIgnoreTeammates)
{
	if (!pLocal || !pProjectile)
		return false;

	const bool bIsLocal = IsOwnedByLocalCached(pLocal, pProjectile);

	if (bIgnoreLocal && bIsLocal)
		return false;

	if (!bIsLocal)
	{
		if (bIgnoreEnemies && pProjectile->m_iTeamNum() != pLocal->m_iTeamNum())
			return false;

		if (bIgnoreTeammates && pProjectile->m_iTeamNum() == pLocal->m_iTeamNum())
			return false;
	}

	return true;
}

Color_t CVisualUtils::GetAlphaColor(Color_t base, float alpha)
{
	base.a = static_cast<byte>(alpha * 255.f);
	return base;
}

Color_t CVisualUtils::GetEntityColor(C_TFPlayer* pLocal, C_BaseEntity* pEntity)
{
	if (!pLocal || !pEntity)
		return { 255, 255, 255, 255 };

	ResetFrameCacheIfNeeded(pLocal);

	auto& cache = GetFrameCacheEntry(pEntity, pLocal);

	if (cache.ColorValid)
		return cache.Color;

	Color_t result = { 255, 255, 255, 255 };
	bool bResolved = false;

	if (pEntity->entindex() == G::nTargetIndex)
	{
		result = CFG::Color_Target;
		bResolved = true;
	}

	if (!bResolved && pEntity->GetClassId() == ETFClassIds::CTFPlayer)
	{
		const auto pPlayer = pEntity->As<C_TFPlayer>();

		if (pPlayer->IsInvulnerable())
		{
			result = CFG::Color_Invulnerable;
			bResolved = true;
		}
		else if (pPlayer->IsInvisible())
		{
			result = CFG::Color_Invisible;
			bResolved = true;
		}
		else if (pPlayer != pLocal && pPlayer->IsPlayerOnSteamFriendsList())
		{
			result = CFG::Color_Friend;
			bResolved = true;
		}
		else if (pPlayer != pLocal)
		{
			// TODO: Handle these colors in F::Players
			PlayerPriority info{};
			F::Players->GetInfo(pPlayer->entindex(), info);

			if (info.Cheater)
			{
				result = CFG::Color_Cheater;
				bResolved = true;
			}
			else if (info.RetardLegit)
			{
				result = CFG::Color_RetardLegit;
				bResolved = true;
			}
		}
	}

	if (!bResolved && IsOwnedByLocalCached(pLocal, pEntity))
	{
		result = CFG::Color_Local;
		bResolved = true;
	}

	if (!bResolved && pEntity->m_iTeamNum() == pLocal->m_iTeamNum())
	{
		result = CFG::Color_Teammate;
		bResolved = true;
	}

	if (!bResolved && pEntity->m_iTeamNum() != pLocal->m_iTeamNum())
	{
		result = CFG::Color_Enemy;
	}

	cache.Color = result;
	cache.ColorValid = true;

	return cache.Color;
}

Color_t CVisualUtils::GetHealthColor(int nHealth, int nMaxHealth)
{
	if (nMaxHealth <= 0)
		return { 255, 0, 0, 255 };

	if (nHealth > nMaxHealth)
		return CFG::Color_OverHeal;

	nHealth = std::max(0, std::min(nHealth, nMaxHealth));
	const int r = std::min((510 * (nMaxHealth - nHealth)) / nMaxHealth, 210);
	const int g = std::min((510 * nHealth) / nMaxHealth, 230);
	return { static_cast<byte>(r), static_cast<byte>(g), 50, 255 };
}

Color_t CVisualUtils::GetHealthColorAlt(int nHealth, int nMaxHealth)
{
	nHealth = std::max(0, std::min(nHealth, nMaxHealth));
	const int r = std::min((510 * (nMaxHealth - nHealth)) / nMaxHealth, 255);
	const int g = std::min((510 * nHealth) / nMaxHealth, 230);
	return { static_cast<byte>(r), static_cast<byte>(g), 50, 255 };
}

int CVisualUtils::CreateTextureFromArray(const unsigned char* rgba, int w, int h)
{
	const int nTextureIdOut = I::MatSystemSurface->CreateNewTextureID(true);
	I::MatSystemSurface->DrawSetTextureRGBAEx(nTextureIdOut, rgba, w, h, IMAGE_FORMAT_BGRA8888);
	return nTextureIdOut;
}

int CVisualUtils::CreateTextureFromVTF(const char* name)
{
	const int nTextureIdOut = I::MatSystemSurface->CreateNewTextureID(false);
	I::MatSystemSurface->DrawSetTextureFile(nTextureIdOut, name, 0, true);
	return nTextureIdOut;
}

int CVisualUtils::GetClassIcon(int nClassNum)
{
	//what are arrays

	static int nScout = CreateTextureFromVTF("hud/leaderboard_class_scout.vtf");
	static int nSoldier = CreateTextureFromVTF("hud/leaderboard_class_soldier.vtf");
	static int nPyro = CreateTextureFromVTF("hud/leaderboard_class_pyro.vtf");
	static int nDemoman = CreateTextureFromVTF("hud/leaderboard_class_demo.vtf");
	static int nHeavy = CreateTextureFromVTF("hud/leaderboard_class_heavy.vtf");
	static int nEngineer = CreateTextureFromVTF("hud/leaderboard_class_engineer.vtf");
	static int nMedic = CreateTextureFromVTF("hud/leaderboard_class_medic.vtf");
	static int nSniper = CreateTextureFromVTF("hud/leaderboard_class_sniper.vtf");
	static int nSpy = CreateTextureFromVTF("hud/leaderboard_class_spy.vtf");

	switch (nClassNum)
	{
	case TF_CLASS_SCOUT: return nScout;
	case TF_CLASS_SOLDIER: return nSoldier;
	case TF_CLASS_PYRO: return nPyro;
	case TF_CLASS_DEMOMAN: return nDemoman;
	case TF_CLASS_HEAVYWEAPONS: return nHeavy;
	case TF_CLASS_ENGINEER: return nEngineer;
	case TF_CLASS_MEDIC: return nMedic;
	case TF_CLASS_SNIPER: return nSniper;
	case TF_CLASS_SPY: return nSpy;
	default: break;
	}

	return 0;
}

int CVisualUtils::GetBuildingTextureId(C_BaseObject* pObject)
{
	static int nSentryGunLvl1 = CreateTextureFromVTF("hud/hud_obj_status_sentry_1.vtf");
	static int nSentryGunLvl2 = CreateTextureFromVTF("hud/hud_obj_status_sentry_2.vtf");
	static int nSentryGunLvl3 = CreateTextureFromVTF("hud/hud_obj_status_sentry_3.vtf");
	static int nDispenser = CreateTextureFromVTF("hud/hud_obj_status_dispenser.vtf");
	static int nTeleporter = CreateTextureFromVTF("hud/hud_obj_status_tele_entrance.vtf");

	if (!pObject)
		return 0;

	switch (pObject->GetClassId())
	{
	case ETFClassIds::CObjectSentrygun:
		{
			switch (pObject->m_iUpgradeLevel())
			{
			case 1: return nSentryGunLvl1;
			case 2: return nSentryGunLvl2;
			case 3: return nSentryGunLvl3;
			default: break;
			}

			break;
		}

	case ETFClassIds::CObjectDispenser: return nDispenser;
	case ETFClassIds::CObjectTeleporter: return nTeleporter;

	default: break;
	}

	return 0;
}

int CVisualUtils::GetHealthIconTextureId()
{
	static int nOut = CreateTextureFromVTF("sprites/healbeam.vtf");
	return nOut;
}

int CVisualUtils::GetAmmoIconTextureId()
{
	static int nOut = CreateTextureFromVTF("hud/hud_obj_status_ammo_64");
	return nOut;
}

int CVisualUtils::GetHalloweenGiftTextureId()
{
	static int nOut = CreateTextureFromVTF("models/props_halloween/halloween_gift.vtf");
	return nOut;
}

bool CVisualUtils::IsOnScreen(const C_TFPlayer* pLocal, const C_BaseEntity* pEntity)
{
	if (!pLocal || !pEntity)
		return false;

	ResetFrameCacheIfNeeded(pLocal);

	auto& cache = GetFrameCacheEntry(pEntity, pLocal);

	if (cache.OnScreenValid)
		return cache.OnScreen;

	bool bOnScreen = true;
	const Vec3& vPos = pEntity->GetAbsOrigin();
	if (vPos.DistToSqr(m_vCachedLocalOrigin) > (300.0f * 300.0f))
	{
		Vec3 vScreen = {};

		if (H::Draw->W2S(vPos, vScreen))
		{
			if (vScreen.x < -400
				|| vScreen.x > m_nCachedScreenW + 400
				|| vScreen.y < -400
				|| vScreen.y > m_nCachedScreenH + 400)
				bOnScreen = false;
		}

		else
		{
			bOnScreen = false;
		}
	}

	cache.OnScreen = bOnScreen;
	cache.OnScreenValid = true;

	return bOnScreen;
}

bool CVisualUtils::IsOnScreenNoEntity(const C_TFPlayer* pLocal, const Vec3& vAbsOrigin)
{
	if (!pLocal)
		return false;

	ResetFrameCacheIfNeeded(pLocal);

	const Vec3& vPos = vAbsOrigin;
	if (vPos.DistToSqr(m_vCachedLocalOrigin) > (300.0f * 300.0f))
	{
		Vec3 vScreen = {};

		if (H::Draw->W2S(vPos, vScreen))
		{
			if (vScreen.x < -400
				|| vScreen.x > m_nCachedScreenW + 400
				|| vScreen.y < -400
				|| vScreen.y > m_nCachedScreenH + 400)
				return false;
		}
		else
		{
			return false;
		}
	}

	return true;
}

int CVisualUtils::GetCat(int nFrame)
{
	if (nFrame < 0 || nFrame > 3)
		return 0;

	static const int arrFrames[4] =
	{
		CreateTextureFromArray(Icons::cat_0, 12, 12),
		CreateTextureFromArray(Icons::cat_1, 12, 12),
		CreateTextureFromArray(Icons::cat_2, 12, 12),
		CreateTextureFromArray(Icons::cat_3, 12, 12)
	};

	return arrFrames[nFrame];
}

int CVisualUtils::GetCat2(int nFrame)
{
	if (nFrame < 0 || nFrame > 3)
		return 0;

	static const int arrFrames[4] =
	{
		CreateTextureFromArray(Icons::cat2_0, 12, 12),
		CreateTextureFromArray(Icons::cat2_1, 12, 12),
		CreateTextureFromArray(Icons::cat2_2, 12, 12),
		CreateTextureFromArray(Icons::cat2_3, 12, 12)
	};

	return arrFrames[nFrame];
}

int CVisualUtils::GetCatSleep(int nFrame)
{
	if (nFrame < 0 || nFrame > 3)
		return 0;

	static const int arrFrames[4] =
	{
		CreateTextureFromArray(Icons::cat_sleep0, 16, 8),
		CreateTextureFromArray(Icons::cat_sleep1, 16, 8),
		CreateTextureFromArray(Icons::cat_sleep2, 16, 8),
		CreateTextureFromArray(Icons::cat_sleep3, 16, 8)
	};

	return arrFrames[nFrame];
}

int CVisualUtils::GetCatRun(int nFrame)
{
	if (nFrame < 0 || nFrame > 7)
		return 0;

	static const int arrFrames[8] =
	{
		CreateTextureFromArray(Icons::cat_run0, 20, 13),
		CreateTextureFromArray(Icons::cat_run1, 20, 13),
		CreateTextureFromArray(Icons::cat_run2, 20, 13),
		CreateTextureFromArray(Icons::cat_run3, 20, 13),
		CreateTextureFromArray(Icons::cat_run4, 20, 13),
		CreateTextureFromArray(Icons::cat_run5, 20, 13),
		CreateTextureFromArray(Icons::cat_run6, 20, 13),
		CreateTextureFromArray(Icons::cat_run7, 20, 13)
	};

	return arrFrames[nFrame];
}

Color_t CVisualUtils::Rainbow()
{
	const float t = TICKS_TO_TIME(I::GlobalVars->tickcount);

	const int r = static_cast<int>(std::round(std::cos(I::GlobalVars->realtime + t + 0.0f) * 127.5f + 127.5f));
	const int g = static_cast<int>(std::round(std::cos(I::GlobalVars->realtime + t + 2.0f) * 127.5f + 127.5f));
	const int b = static_cast<int>(std::round(std::cos(I::GlobalVars->realtime + t + 4.0f) * 127.5f + 127.5f));

	return Color_t{ static_cast<byte>(r), static_cast<byte>(g), static_cast<byte>(b), 255 };
}

Color_t CVisualUtils::RainbowTickOffset(int nTick)
{
	const float t = TICKS_TO_TIME(nTick);

	const int r = static_cast<int>(std::lround(std::cos((I::GlobalVars->realtime * 2.0f) + t + 0.0f) * 127.5f + 127.5f));
	const int g = static_cast<int>(std::lround(std::cos((I::GlobalVars->realtime * 2.0f) + t + 2.0f) * 127.5f + 127.5f));
	const int b = static_cast<int>(std::lround(std::cos((I::GlobalVars->realtime * 2.0f) + t + 4.0f) * 127.5f + 127.5f));

	return Color_t{ static_cast<byte>(r), static_cast<byte>(g), static_cast<byte>(b), 255 };
}
