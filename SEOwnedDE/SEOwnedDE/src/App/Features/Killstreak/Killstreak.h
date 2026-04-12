#pragma once

#include "../../../SDK/SDK.h"

class CKillstreak
{
private:
    std::unordered_map<int, int> m_mKillstreakMap = {};
    int m_iCurrentKillstreak = 0;

private:
    int GetCurrentStreak() const;
    void ApplyKillstreak(int iLocalIdx);

public:
    void PlayerDeath(IGameEvent* pEvent);
    void PlayerSpawn(IGameEvent* pEvent);
    void Reset();
};

MAKE_SINGLETON_SCOPED(CKillstreak, Killstreak, F);
