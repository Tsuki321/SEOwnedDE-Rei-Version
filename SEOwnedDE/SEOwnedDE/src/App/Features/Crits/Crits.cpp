#include "Crits.h"

#include "../CFG.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace
{
	constexpr int SEED_ATTEMPTS = 4096;
	constexpr int BUCKET_ATTEMPTS = 1000;
	constexpr int SLOT_MELEE = 2;

	bool IsFiring(const CUserCmd* pCmd, C_TFWeaponBase* weapon)
	{
		if (weapon->GetSlot() != SLOT_MELEE)
		{
			if (!weapon->HasPrimaryAmmoForShot())
				return false;

			const int nWeaponID = weapon->GetWeaponID();

			if (nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER || nWeaponID == TF_WEAPON_CANNON)
				return (G::nOldButtons & IN_ATTACK) && !(pCmd->buttons & IN_ATTACK);

			if (weapon->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka)
				return G::bCanPrimaryAttack;
		}

		return (pCmd->buttons & IN_ATTACK) && G::bCanPrimaryAttack;
	}

}

int FindCritCmd(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon, bool bCrit)
{
	const int nBackupRandomSeed = *SDKUtils::RandomSeed();
	int nCommandNumber = std::max(1, pCmd->command_number);

	for (int n = 0; n < SEED_ATTEMPTS; n++)
	{
		*SDKUtils::RandomSeed() = MD5_PseudoRandom(nCommandNumber) & std::numeric_limits<int>::max();

		bool bCalc = false;

		if (pWeapon->GetSlot() == SLOT_MELEE)
		{
			bCalc = pWeapon->CalcIsAttackCriticalHelperMelee();
		}
		else
		{
			bCalc = pWeapon->CalcIsAttackCriticalHelper();
		}

		if (bCrit ? bCalc : !bCalc)
		{
			*SDKUtils::RandomSeed() = nBackupRandomSeed;
			return nCommandNumber;
		}

		nCommandNumber++;
	}

	*SDKUtils::RandomSeed() = nBackupRandomSeed;
	return 0;
}

int CCrits::CommandToSeed(int nCommandNumber) const
{
	const int nSeed = MD5_PseudoRandom(nCommandNumber) & std::numeric_limits<int>::max();
	const int nLocalIndex = std::max(1, I::EngineClient->GetLocalPlayer());
	const int nMask = m_bMelee ? (m_iEntIndex << 16) | (nLocalIndex << 8) : (m_iEntIndex << 8) | nLocalIndex;
	return nSeed ^ nMask;
}

void CCrits::UpdateWeaponInfo(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	m_iEntIndex = pWeapon->entindex();
	m_bMelee = pWeapon->GetSlot() == SLOT_MELEE;

	if (m_bMelee)
	{
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE_MELEE * pLocal->GetCritMult();
	}
	else if (pWeapon->IsRapidFire())
	{
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE_RAPID * pLocal->GetCritMult();

		const float flNonCritDuration = (TF_DAMAGE_CRIT_DURATION_RAPID / m_flCritChance) - TF_DAMAGE_CRIT_DURATION_RAPID;
		m_flCritChance = flNonCritDuration > 0.0f ? 1.0f / flNonCritDuration : 0.0f;
	}
	else
	{
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE * pLocal->GetCritMult();
	}

	m_flMultCritChance = SDKUtils::AttribHookValue(1.0f, "mult_crit_chance", pWeapon);
	m_flCritChance *= m_flMultCritChance;

	static auto tf_weapon_criticals_bucket_cap = I::CVar->FindVar("tf_weapon_criticals_bucket_cap");
	if (!tf_weapon_criticals_bucket_cap)
	{
		m_flDamage = 0.0f;
		m_flCost = 0.0f;
		m_iPotentialCrits = 0;
		m_iAvailableCrits = 0;
		m_iNextCrit = 0;
		return;
	}

	const float flBucketCap = std::max(0.0f, tf_weapon_criticals_bucket_cap->GetFloat());
	const float flBucket = std::max(0.0f, pWeapon->m_flCritTokenBucket());
	const int iCritChecks = std::max(0, pWeapon->m_nCritChecks());
	const int iCritSeedRequests = std::max(0, pWeapon->m_nCritSeedRequests());

	const bool bRapidFire = pWeapon->IsRapidFire();
	const float flFireRate = std::max(0.001f, pWeapon->GetFireRate());

	float flDamage = std::max(0.0f, pWeapon->GetDamage());
	int nProjectilesPerShot = pWeapon->GetBulletsPerShot(false);

	if (!m_bMelee && nProjectilesPerShot > 0)
	{
		nProjectilesPerShot = static_cast<int>(SDKUtils::AttribHookValue(static_cast<float>(nProjectilesPerShot), "mult_bullets_per_shot", pWeapon));
	}
	else
	{
		nProjectilesPerShot = 1;
	}

	nProjectilesPerShot = std::max(1, nProjectilesPerShot);
	const float flBaseDamage = flDamage *= static_cast<float>(nProjectilesPerShot);

	if (bRapidFire)
	{
		flDamage *= TF_DAMAGE_CRIT_DURATION_RAPID / flFireRate;

		if (flDamage * TF_DAMAGE_CRIT_MULTIPLIER > flBucketCap)
		{
			flDamage = flBucketCap / TF_DAMAGE_CRIT_MULTIPLIER;
		}
	}

	const float flRatio = static_cast<float>(iCritSeedRequests + 1) / static_cast<float>(iCritChecks + 1);
	const float flMult = m_bMelee ? 0.5f : Math::RemapVal(flRatio, 0.1f, 1.0f, 1.0f, 3.0f);
	const float flCost = flDamage * TF_DAMAGE_CRIT_MULTIPLIER;

	int iPotentialCrits = 0;
	const float flPotentialDenominator = (TF_DAMAGE_CRIT_MULTIPLIER * flDamage / (m_bMelee ? 2.0f : 1.0f)) - flBaseDamage;

	if (flPotentialDenominator > 0.0f)
	{
		iPotentialCrits = static_cast<int>((std::max(flBucketCap, flBucket) - flBaseDamage) / flPotentialDenominator);
		iPotentialCrits = std::max(0, iPotentialCrits);
	}

	int iAvailableCrits = 0;
	{
		int iTestShots = iCritChecks;
		int iTestCrits = iCritSeedRequests;
		float flTestBucket = flBucket;

		for (int i = 0; i < BUCKET_ATTEMPTS; i++)
		{
			iTestShots++;
			iTestCrits++;

			const float flTestMult = m_bMelee ? 0.5f : Math::RemapVal(static_cast<float>(iTestCrits) / static_cast<float>(std::max(1, iTestShots)), 0.1f, 1.0f, 1.0f, 3.0f);

			if (flTestBucket < flBucketCap)
			{
				flTestBucket = std::min(flTestBucket + flBaseDamage, flBucketCap);
			}

			flTestBucket -= flCost * flTestMult;

			if (flTestBucket < 0.0f)
			{
				break;
			}

			iAvailableCrits++;
		}
	}

	int iNextCrit = 0;
	if (iAvailableCrits != iPotentialCrits)
	{
		int iTestShots = iCritChecks;
		int iTestCrits = iCritSeedRequests;
		float flTestBucket = flBucket;
		float flTickBase = I::GlobalVars->curtime;
		float flLastRapidFireCritCheckTime = pWeapon->m_flLastRapidFireCritCheckTime();

		for (int i = 0; i < BUCKET_ATTEMPTS; i++)
		{
			int iCrits = 0;
			{
				int iTestShots2 = iTestShots;
				int iTestCrits2 = iTestCrits;
				float flTestBucket2 = flTestBucket;

				for (int j = 0; j < BUCKET_ATTEMPTS; j++)
				{
					iTestShots2++;
					iTestCrits2++;

					const float flTestMult = m_bMelee ? 0.5f : Math::RemapVal(static_cast<float>(iTestCrits2) / static_cast<float>(std::max(1, iTestShots2)), 0.1f, 1.0f, 1.0f, 3.0f);

					if (flTestBucket2 < flBucketCap)
					{
						flTestBucket2 = std::min(flTestBucket2 + flBaseDamage, flBucketCap);
					}

					flTestBucket2 -= flCost * flTestMult;

					if (flTestBucket2 < 0.0f)
					{
						break;
					}

					iCrits++;
				}
			}

			if (iAvailableCrits < iCrits)
			{
				break;
			}

			if (!bRapidFire)
			{
				iTestShots++;
			}
			else
			{
				flTickBase += std::ceilf(flFireRate / TICK_INTERVAL) * TICK_INTERVAL;

				if (flTickBase >= flLastRapidFireCritCheckTime + 1.0f || (!i && flTestBucket == flBucketCap))
				{
					iTestShots++;
					flLastRapidFireCritCheckTime = flTickBase;
				}
			}

			if (flTestBucket < flBucketCap)
			{
				flTestBucket = std::min(flTestBucket + flBaseDamage, flBucketCap);
			}

			iNextCrit++;
		}
	}

	m_flDamage = flBaseDamage;
	m_flCost = flCost * flMult;
	m_iPotentialCrits = iPotentialCrits;
	m_iAvailableCrits = iAvailableCrits;
	m_iNextCrit = iNextCrit;
}

void CCrits::UpdateInfo(C_TFPlayer* pLocal, C_TFWeaponBase* pWeapon)
{
	UpdateWeaponInfo(pLocal, pWeapon);

	m_bCritBanned = false;
	m_flDamageTilFlip = 0.0f;

	if (!m_bMelee)
	{
		const float flNormalizedDamage = static_cast<float>(m_iCritDamage) / TF_DAMAGE_CRIT_MULTIPLIER;
		const float flCritChance = m_flCritChance + 0.1f;

		if (m_iRangedDamage > 0 && m_iCritDamage > 0)
		{
			const float flObservedDenominator = flNormalizedDamage + static_cast<float>(m_iRangedDamage - m_iCritDamage);

			if (flObservedDenominator > 0.0f)
			{
				const float flObservedCritChance = flNormalizedDamage / flObservedDenominator;
				m_bCritBanned = flObservedCritChance > flCritChance;
			}
		}

		if (m_iCritDamage > 0 && flCritChance > 0.0f && flCritChance < 1.0f)
		{
			if (m_bCritBanned)
			{
				m_flDamageTilFlip = flNormalizedDamage / flCritChance + flNormalizedDamage * 2.0f - static_cast<float>(m_iRangedDamage);
			}
			else
			{
				m_flDamageTilFlip = TF_DAMAGE_CRIT_MULTIPLIER * (flNormalizedDamage - flCritChance * (flNormalizedDamage + m_iRangedDamage - m_iCritDamage)) / (flCritChance - 1.0f);
			}
		}
	}

	if (const auto pResource = GetTFPlayerResource())
	{
		const int nLocalIndex = I::EngineClient->GetLocalPlayer();

		if (nLocalIndex > 0)
		{
			static int nDamageOffset = NetVars::GetNetVar("CTFPlayerResource", "m_iDamage");
			auto* pDamage = reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(pResource) + nDamageOffset);
			m_iResourceDamage = pDamage[nLocalIndex];
		}

		m_iDesyncDamage = m_iRangedDamage + m_iMeleeDamage - m_iResourceDamage;
	}
}

CCrits::ECritRequest CCrits::GetCritRequest(const CUserCmd* pCmd, C_TFWeaponBase* pWeapon)
{
	bool bCanCrit = m_iAvailableCrits > 0 && !m_bCritBanned;
	bool bPressed = H::Input->IsDown(m_bMelee ? CFG::Exploits_Crits_Force_Crit_Key_Melee : CFG::Exploits_Crits_Force_Crit_Key);

	if (bPressed && m_bMelee)
	{
		if (const auto pLocal = H::Entities->GetLocal())
		{
			if (G::nTargetIndex)
			{
				const auto pEntity = I::ClientEntityList->GetClientEntity(G::nTargetIndex);

				if (pEntity && pEntity->GetClassId() == ETFClassIds::CTFPlayer && pEntity->As<C_TFPlayer>()->m_iTeamNum() == pLocal->m_iTeamNum())
				{
					bPressed = false;
				}
			}
		}
	}

	const bool bSkip = CFG::Exploits_Crits_Skip_Random_Crits;
	const bool bDesync = CommandToSeed(pCmd->command_number) == pWeapon->m_iCurrentSeed();

	if (bCanCrit && bPressed)
	{
		return ECritRequest::Crit;
	}

	if (bSkip || bDesync)
	{
		return ECritRequest::Skip;
	}

	return ECritRequest::Any;
}

bool CCrits::WeaponCanCrit(C_TFWeaponBase* pWeapon, bool bWeaponOnly) const
{
	if ((!bWeaponOnly && !pWeapon->AreRandomCritsEnabled()) || SDKUtils::AttribHookValue(1.0f, "mult_crit_chance", pWeapon) <= 0.0f)
	{
		return false;
	}

	switch (pWeapon->GetWeaponID())
	{
		case TF_WEAPON_PDA:
		case TF_WEAPON_PDA_ENGINEER_BUILD:
		case TF_WEAPON_PDA_ENGINEER_DESTROY:
		case TF_WEAPON_PDA_SPY:
		case TF_WEAPON_PDA_SPY_BUILD:
		case TF_WEAPON_BUILDER:
		case TF_WEAPON_INVIS:
		case TF_WEAPON_JAR_MILK:
		case TF_WEAPON_LUNCHBOX:
		case TF_WEAPON_BUFF_ITEM:
		case TF_WEAPON_FLAME_BALL:
		case TF_WEAPON_ROCKETPACK:
		case TF_WEAPON_JAR_GAS:
		case TF_WEAPON_LASER_POINTER:
		case TF_WEAPON_MEDIGUN:
		case TF_WEAPON_SNIPERRIFLE:
		case TF_WEAPON_SNIPERRIFLE_DECAP:
		case TF_WEAPON_SNIPERRIFLE_CLASSIC:
		case TF_WEAPON_COMPOUND_BOW:
		case TF_WEAPON_JAR:
		case TF_WEAPON_KNIFE:
		case TF_WEAPON_PASSTIME_GUN:
			return false;
		default:
			break;
	}

	return true;
}

void CCrits::Run(CUserCmd* pCmd)
{
	const auto pLocal = H::Entities->GetLocal();

	if (!pLocal || pLocal->deadflag())
		return;

	static auto tf_weapon_criticals = I::CVar->FindVar("tf_weapon_criticals");

	if (!tf_weapon_criticals || !tf_weapon_criticals->GetInt())
	{
		return;
	}

	const auto pWeapon = H::Entities->GetWeapon();

	if (!pWeapon || pWeapon->GetWeaponID() == TF_WEAPON_KNIFE || !IsFiring(pCmd, pWeapon))
		return;

	UpdateInfo(pLocal, pWeapon);

	if (pLocal->IsCritBoosted() || pLocal->IsMiniCritBoosted() || pWeapon->m_flCritTime() > I::GlobalVars->curtime || !WeaponCanCrit(pWeapon))
	{
		return;
	}

	if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN && (pCmd->buttons & IN_ATTACK))
	{
		pCmd->buttons &= ~IN_ATTACK2;
	}

	const auto request = GetCritRequest(pCmd, pWeapon);

	if (request == ECritRequest::Any)
	{
		return;
	}

	const bool bWantCrit = request == ECritRequest::Crit;

	if (const int nCommand = FindCritCmd(pCmd, pWeapon, bWantCrit))
	{
		pCmd->command_number = nCommand;
		pCmd->random_seed = MD5_PseudoRandom(nCommand) & std::numeric_limits<int>::max();
	}
}

void CCrits::Paint()
{
	if (!CFG::Exploits_Crits_Draw_Indicator || I::EngineVGui->IsGameUIVisible() || SDKUtils::BInEndOfMatch())
	{
		return;
	}

	if (CFG::Misc_Clean_Screenshot && I::EngineClient->IsTakingScreenshot())
	{
		return;
	}

	static auto tf_weapon_criticals = I::CVar->FindVar("tf_weapon_criticals");
	if (!tf_weapon_criticals || !tf_weapon_criticals->GetInt())
	{
		return;
	}

	const auto pLocal = H::Entities->GetLocal();
	const auto pWeapon = H::Entities->GetWeapon();

	if (!pLocal || !pWeapon || pLocal->deadflag() || !WeaponCanCrit(pWeapon, true))
	{
		return;
	}

	UpdateInfo(pLocal, pWeapon);

	const int nScreenW = H::Draw->GetScreenW();
	const int nScreenH = H::Draw->GetScreenH();

	if (nScreenW < 1 || nScreenH < 1)
	{
		return;
	}

	const int nBoxW = 186;
	const int nBoxH = 30;
	const int nBarH = 3;
	const int nTextAreaH = nBoxH - nBarH;

	const int x = (nScreenW / 2) - (nBoxW / 2);
	const int y = (nScreenH / 2) + 116;

	Color_t statusColor = {200, 200, 200, 255};
	Color_t barColor = CFG::Menu_Accent_Secondary;
	float flProgress = 0.0f;

	std::string topText = std::format("Crits: {} / {}", std::max(0, m_iAvailableCrits), std::max(0, m_iPotentialCrits));
	std::string bottomText = "";

	if (pLocal->IsCritBoosted())
	{
		bottomText = "BOOSTED";
		statusColor = {100, 255, 255, 255};
		barColor = statusColor;
		flProgress = 1.0f;
	}
	else if (pWeapon->m_flCritTime() > I::GlobalVars->curtime)
	{
		const float flTime = pWeapon->m_flCritTime() - I::GlobalVars->curtime;
		bottomText = "STREAMING";
		statusColor = {100, 255, 255, 255};
		barColor = statusColor;
		flProgress = std::clamp(flTime / TF_DAMAGE_CRIT_DURATION_RAPID, 0.0f, 1.0f);
	}
	else if (m_bCritBanned && !m_bMelee)
	{
		topText = std::format("DMG: {}", static_cast<int>(std::ceil(std::max(0.0f, m_flDamageTilFlip))));
		bottomText = "BANNED";
		statusColor = {200, 60, 60, 255};
		barColor = statusColor;
		flProgress = 0.12f;
	}
	else if (pWeapon->IsRapidFire() && I::GlobalVars->curtime < pWeapon->m_flLastRapidFireCritCheckTime() + 1.0f)
	{
		const float flTime = std::max(0.0f, (pWeapon->m_flLastRapidFireCritCheckTime() + 1.0f) - I::GlobalVars->curtime);
		bottomText = std::format("WAIT {:.2f}s", flTime);
		statusColor = {255, 180, 60, 255};
		barColor = statusColor;
		flProgress = 1.0f - std::clamp(flTime, 0.0f, 1.0f);
	}
	else if (m_iAvailableCrits > 0)
	{
		bottomText = "READY";
		statusColor = {64, 214, 114, 255};
		barColor = statusColor;
		flProgress = 1.0f;
	}
	else
	{
		bottomText = std::format("DMG: {}", static_cast<int>(std::ceil(std::max(0.0f, m_flCost - pWeapon->m_flCritTokenBucket()))));
		statusColor = {200, 200, 200, 255};

		static auto tf_weapon_criticals_bucket_cap = I::CVar->FindVar("tf_weapon_criticals_bucket_cap");
		if (tf_weapon_criticals_bucket_cap)
		{
			const float flBucketCap = std::max(1.0f, tf_weapon_criticals_bucket_cap->GetFloat());
			flProgress = std::clamp(pWeapon->m_flCritTokenBucket() / flBucketCap, 0.0f, 1.0f);
		}
	}

	H::Draw->Rect(x, y, nBoxW, nTextAreaH, {0, 0, 0, 180});
	H::Draw->OutlinedRect(x, y, nBoxW, nTextAreaH, CFG::Menu_Outline);

	H::Draw->Rect(x, y + nTextAreaH, nBoxW, nBarH, {0, 0, 0, 180});

	const int nFillW = static_cast<int>(std::round(static_cast<float>(nBoxW) * std::clamp(flProgress, 0.0f, 1.0f)));
	if (nFillW > 0)
	{
		const Color_t barStart{barColor.r, barColor.g, barColor.b, 80};
		H::Draw->GradientRect(x, y + nTextAreaH, nFillW, nBarH, barStart, barColor, true);
	}

	H::Draw->String(H::Fonts->Get(EFonts::ESP_SMALL), x + (nBoxW / 2), y + 4, {220, 220, 220, 255}, POS_CENTERX, topText.c_str());
	H::Draw->String(H::Fonts->Get(EFonts::ESP_SMALL), x + (nBoxW / 2), y + 14, statusColor, POS_CENTERX, bottomText.c_str());
}

void CCrits::Event(IGameEvent* pEvent, std::uint32_t eventHash)
{
	if (!pEvent)
	{
		return;
	}

	static constexpr auto player_hurt = HASH_CT("player_hurt");
	static constexpr auto scorestats_accumulated_update = HASH_CT("scorestats_accumulated_update");
	static constexpr auto mvm_reset_stats = HASH_CT("mvm_reset_stats");
	static constexpr auto client_beginconnect = HASH_CT("client_beginconnect");
	static constexpr auto client_disconnect = HASH_CT("client_disconnect");
	static constexpr auto game_newmap = HASH_CT("game_newmap");

	if (eventHash == player_hurt)
	{
		const auto pLocal = H::Entities->GetLocal();

		if (!pLocal)
		{
			return;
		}

		const int iVictim = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
		const int iAttacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));

		if (iVictim == iAttacker || iAttacker != I::EngineClient->GetLocalPlayer())
		{
			return;
		}

		const int iDamage = pEvent->GetInt("damageamount");
		if (iDamage <= 0)
		{
			return;
		}

		const bool bCrit = pEvent->GetBool("crit") || pEvent->GetBool("minicrit");
		const int iWeaponID = pEvent->GetInt("weaponid");

		C_TFWeaponBase* pWeapon = nullptr;
		for (int i = 0; i < MAX_WEAPONS; i++)
		{
			auto pWeaponCandidate = pLocal->GetWeaponFromSlot(i);
			if (!pWeaponCandidate || pWeaponCandidate->GetWeaponID() != iWeaponID)
			{
				continue;
			}

			pWeapon = pWeaponCandidate;
			break;
		}

		if (!pWeapon || pWeapon->GetSlot() != SLOT_MELEE)
		{
			m_iRangedDamage += iDamage;

			if (bCrit && !pLocal->IsCritBoosted())
			{
				m_iCritDamage += iDamage;
			}
		}
		else
		{
			m_iMeleeDamage += iDamage;
		}

		return;
	}

	if (eventHash == scorestats_accumulated_update || eventHash == mvm_reset_stats)
	{
		m_iRangedDamage = 0;
		m_iCritDamage = 0;
		m_iMeleeDamage = 0;
		return;
	}

	if (eventHash == client_beginconnect || eventHash == client_disconnect || eventHash == game_newmap)
	{
		Reset();
	}
}

void CCrits::Reset()
{
	m_iCritDamage = 0;
	m_iRangedDamage = 0;
	m_iMeleeDamage = 0;
	m_iResourceDamage = 0;
	m_iDesyncDamage = 0;

	m_bCritBanned = false;
	m_flDamageTilFlip = 0.0f;
	m_flDamage = 0.0f;
	m_flCost = 0.0f;
	m_iAvailableCrits = 0;
	m_iPotentialCrits = 0;
	m_iNextCrit = 0;
	m_iEntIndex = 0;
	m_bMelee = false;
	m_flCritChance = 0.0f;
	m_flMultCritChance = 1.0f;
	}
}
