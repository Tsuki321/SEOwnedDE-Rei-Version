#pragma once
#include "../../../SDK/SDK.h"
#include "CosmeticInventoryPersistence.h"
#include <atomic>

class CLocalCosmetics
{
    struct RuntimeSlot
    {
        std::uint64_t item = 0;
        CBaseHandle entity;
        int model = -1;
        bool attempted = false;
    };
    SimulatedCosmetics::SavedState m_state;
    std::vector<SimulatedCosmetics::Definition> m_catalog;
    std::array<RuntimeSlot, SimulatedCosmetics::Slots> m_runtime;
    CBaseHandle m_owner;
    int m_class = 0;
    int m_team = 0;
    std::mt19937_64 m_random{std::random_device{}()};
    std::string m_status;
    std::string m_runtimeStatus;
    bool m_loadFailed = false;
    bool m_refreshRuntime = false;
    std::atomic<bool> m_frameSeen = false;
    std::atomic<bool> m_shutdownRequested = false;
    std::atomic<bool> m_shutdownDone = false;

    void ReleaseSlot(RuntimeSlot& slot);
    void ResetRuntime();
public:
    void Load();
    bool Save();
    void RefreshCatalog();
    void Run();
    void LevelShutdown();
    void Shutdown();
    bool ShouldHideOriginal(C_BaseEntity* entity) const;

    const auto& Catalog() const { return m_catalog; }
    const auto& Inventory() const { return m_state.inventory; }
    bool Enabled() const { return m_state.enabled; }
    bool HideOriginals() const { return m_state.hideOriginals; }
    void SetEnabled(bool enabled);
    void SetHideOriginals(bool hide);
    const auto& Status() const { return m_status; }
    const auto& RuntimeStatus() const { return m_runtimeStatus; }
    std::string Name(int definition) const;
    std::uint64_t OpenCrate(int playerClass);
    bool Equip(std::uint64_t id, int playerClass, int slot);
    void Unequip(int playerClass, int slot);
    void Remove(std::uint64_t id);
};

MAKE_SINGLETON_SCOPED(CLocalCosmetics, LocalCosmetics, F);
