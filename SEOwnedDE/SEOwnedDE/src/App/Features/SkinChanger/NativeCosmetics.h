#pragma once
#include "CosmeticInventory.h"

// All engine calls run on the client frame thread. These objects never enter
// Steam's shared-object cache; owned Steam items keep the original code path.
class CNativeCosmetics
{
    struct Entry
    {
        void* object = nullptr;
        int position = 0;
        std::array<int, 10> equipped{};
    };
    std::map<std::uint64_t, Entry> m_items;
    void* m_inventory = nullptr;
    std::uint64_t m_owner = 0;
    int m_capacity = 0;
    bool m_ready = false;
    bool m_attempted = false;
    std::string m_status;
    bool Initialize();
    void Clear();
public:
    void Run(bool enabled, const SimulatedCosmetics::Inventory& inventory, bool expand = false);
    void Shutdown();
    void* ItemData(void* inventory, std::uint64_t id) const;
    void* Loadout(void* inventory, int playerClass, int slot) const;
    bool Equip(void* manager, int playerClass, int slot, std::uint64_t id, bool& result);
    int ExtraCapacity(void* inventory) const;
    const auto& Status() const { return m_status; }
};

inline CNativeCosmetics g_NativeCosmetics;
