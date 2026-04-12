#include "Killstreak.h"

#include "../CFG.h"

namespace
{
    constexpr int STREAK_TYPE_COUNT = kTFStreak_COUNT;
}

int CKillstreak::GetCurrentStreak() const
{
    return m_iCurrentKillstreak;
}

void CKillstreak::ApplyKillstreak(int iLocalIdx)
{
    if (iLocalIdx <= 0)
        return;

    const auto pLocal = H::Entities->GetLocal();
    const auto pResource = GetTFPlayerResource();

    if (!pLocal || !pResource)
        return;

    const int iCurrentStreak = GetCurrentStreak();

    static int nResourceStreakOffset = NetVars::GetNetVar("CTFPlayerResource", "m_iStreaks");
    auto* pResourceStreaks = reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(pResource) + nResourceStreakOffset);

    const int nPlayerStreakOffset = iLocalIdx * STREAK_TYPE_COUNT;
    pResourceStreaks[nPlayerStreakOffset + kTFStreak_Kills] = iCurrentStreak;
    pResourceStreaks[nPlayerStreakOffset + kTFStreak_KillsAll] = iCurrentStreak;
    pResourceStreaks[nPlayerStreakOffset + kTFStreak_Ducks] = iCurrentStreak;
    pResourceStreaks[nPlayerStreakOffset + kTFStreak_Duck_levelup] = iCurrentStreak;

    static int nLocalStreakOffset = NetVars::GetNetVar("CTFPlayer", "m_nStreaks");
    auto* pLocalStreaks = reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(pLocal) + nLocalStreakOffset);

    pLocalStreaks[kTFStreak_Kills] = iCurrentStreak;
    pLocalStreaks[kTFStreak_KillsAll] = iCurrentStreak;
    pLocalStreaks[kTFStreak_Ducks] = iCurrentStreak;
    pLocalStreaks[kTFStreak_Duck_levelup] = iCurrentStreak;
}

void CKillstreak::PlayerDeath(IGameEvent* pEvent)
{
    if (!CFG::Visuals_Killstreak_Weapons || !pEvent)
        return;

    const int attacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
    const int userid = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
    const int iLocalPlayerIdx = I::EngineClient->GetLocalPlayer();

    if (userid == iLocalPlayerIdx)
    {
        Reset();
        return;
    }

    const auto pLocal = H::Entities->GetLocal();

    if (attacker != iLocalPlayerIdx || attacker == userid || !pLocal)
        return;

    if (pLocal->deadflag())
    {
        if (m_iCurrentKillstreak)
            Reset();

        return;
    }

    const int iWeaponID = pEvent->GetInt("weaponid");

    m_iCurrentKillstreak++;
    m_mKillstreakMap[iWeaponID]++;

    pEvent->SetInt("kill_streak_total", GetCurrentStreak());
    pEvent->SetInt("kill_streak_wep", m_mKillstreakMap[iWeaponID]);

    ApplyKillstreak(iLocalPlayerIdx);
}

void CKillstreak::PlayerSpawn(IGameEvent* pEvent)
{
    if (!CFG::Visuals_Killstreak_Weapons || !pEvent)
        return;

    const int iLocalPlayerIdx = I::EngineClient->GetLocalPlayer();

    if (I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid")) != iLocalPlayerIdx)
        return;

    Reset();
    ApplyKillstreak(iLocalPlayerIdx);
}

void CKillstreak::Reset()
{
    m_iCurrentKillstreak = 0;
    m_mKillstreakMap.clear();
}
