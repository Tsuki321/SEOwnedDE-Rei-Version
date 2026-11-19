#include "NativeCosmetics.h"
#include "NativeCosmeticBuild.h"
#include "LocalCosmetics.h"
#include "../../../Utils/HookManager/HookManager.h"
#include <cstring>

namespace
{
    using ManagerFn = void* (__fastcall*)();
    using CreateFn = void* (__fastcall*)();
    using AddFn = bool (__fastcall*)(void*, void*, bool, bool, bool);
    using FindFn = void* (__fastcall*)(void*, std::uint64_t, int*);
    using NotifyFn = void (__fastcall*)(void*);
    using EquipObjectFn = void (__fastcall*)(void*, std::uint16_t, std::uint16_t);
    using DataFn = void* (__fastcall*)(void*, std::uint64_t);
    using LoadoutFn = void* (__fastcall*)(void*, int, int);
    using EquipFn = bool (__fastcall*)(void*, int, int, std::uint64_t);
    using CapacityFn = int (__fastcall*)(void*);
    ManagerFn manager = nullptr;
    CreateFn create = nullptr;
    AddFn add = nullptr;
    FindFn find = nullptr;
    NotifyFn notify = nullptr;
    EquipObjectFn equipObject = nullptr;
    DataFn originalData = nullptr;
    LoadoutFn originalLoadout = nullptr;
    EquipFn originalEquip = nullptr;
    CapacityFn originalCapacity = nullptr;
    std::array<void*, 4> hookTargets{};

    template<class T> T& Field(void* object, std::size_t offset)
    {
        return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
    }
    void* __fastcall DataHook(void* inventory, std::uint64_t id)
    {
        if (auto item = g_NativeCosmetics.ItemData(inventory, id)) return item;
        return originalData(inventory, id);
    }
    void* __fastcall LoadoutHook(void* inventory, int playerClass, int slot)
    {
        if (auto item = g_NativeCosmetics.Loadout(inventory, playerClass, slot)) return item;
        return originalLoadout(inventory, playerClass, slot);
    }
    bool __fastcall EquipHook(void* self, int playerClass, int slot, std::uint64_t id)
    {
        bool result = false;
        if (g_NativeCosmetics.Equip(self, playerClass, slot, id, result)) return result;
        return originalEquip(self, playerClass, slot, id);
    }
    int __fastcall CapacityHook(void* inventory)
    {
        return originalCapacity(inventory) + g_NativeCosmetics.ExtraCapacity(inventory);
    }
    void Destroy(void* object)
    {
        reinterpret_cast<void (__fastcall*)(void*, unsigned char)>(Memory::GetVFunc(object, 0))(object, 1);
    }
    void Remove(void* inventory, std::uint64_t id)
    {
        // CPlayerInventory::RemoveItem deletes the view and dirties its handles.
        reinterpret_cast<void (__fastcall*)(void*, std::uint64_t)>(Memory::GetVFunc(inventory, 19))(inventory, id);
    }
}

bool CNativeCosmetics::Initialize()
{
    if (m_attempted) return m_ready;
    m_attempted = true;
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"client.dll"));
    if (!base) return false;
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (pe->FileHeader.TimeDateStamp != NativeCosmeticBuild::Timestamp
        || pe->OptionalHeader.SizeOfImage != NativeCosmeticBuild::ImageSize)
    {
        m_status = "Native backpack unavailable: TF2 build needs verification";
        return false;
    }
    const auto resolve = [base](const NativeCosmeticBuild::Function& spec) -> void*
    {
        const auto address = reinterpret_cast<void*>(base + spec.rva);
        return std::memcmp(address, spec.bytes.data(), spec.bytes.size()) == 0 ? address : nullptr;
    };
    manager = reinterpret_cast<ManagerFn>(resolve(NativeCosmeticBuild::manager));
    create = reinterpret_cast<CreateFn>(resolve(NativeCosmeticBuild::create));
    add = reinterpret_cast<AddFn>(resolve(NativeCosmeticBuild::add));
    find = reinterpret_cast<FindFn>(resolve(NativeCosmeticBuild::find));
    notify = reinterpret_cast<NotifyFn>(resolve(NativeCosmeticBuild::notify));
    equipObject = reinterpret_cast<EquipObjectFn>(resolve(NativeCosmeticBuild::equip));
    hookTargets = {resolve(NativeCosmeticBuild::soc), resolve(NativeCosmeticBuild::loadout),
        resolve(NativeCosmeticBuild::equip_ui), resolve(NativeCosmeticBuild::capacity)};
    if (!manager || !create || !add || !find || !notify || !equipObject
        || std::any_of(hookTargets.begin(), hookTargets.end(), [](auto p) { return !p; }))
    {
        m_status = "Native backpack unavailable: inventory code changed";
        return false;
    }
    const std::array<void*, 4> replacements = {reinterpret_cast<void*>(DataHook),
        reinterpret_cast<void*>(LoadoutHook), reinterpret_cast<void*>(EquipHook), reinterpret_cast<void*>(CapacityHook)};
    const std::array<void**, 4> originals = {reinterpret_cast<void**>(&originalData),
        reinterpret_cast<void**>(&originalLoadout), reinterpret_cast<void**>(&originalEquip), reinterpret_cast<void**>(&originalCapacity)};
    std::size_t created = 0;
    for (; created < hookTargets.size(); ++created)
        if (MH_CreateHook(hookTargets[created], replacements[created], originals[created]) != MH_OK) break;
    if (created == hookTargets.size())
    {
        m_ready = true;
        for (auto target : hookTargets)
            if (MH_EnableHook(target) != MH_OK) { m_ready = false; break; }
    }
    if (!m_ready)
    {
        for (std::size_t i = 0; i < created; ++i) { MH_DisableHook(hookTargets[i]); MH_RemoveHook(hookTargets[i]); }
        m_status = "Native backpack hooks unavailable";
    }
    return m_ready;
}

void CNativeCosmetics::Clear()
{
    for (const auto& [id, entry] : m_items)
    {
        if (m_inventory && find(m_inventory, id, nullptr)) Remove(m_inventory, id);
        Destroy(entry.object);
    }
    m_items.clear();
    const auto previousInventory = m_inventory;
    m_inventory = nullptr;
    m_owner = 0;
    m_capacity = 0;
    if (previousInventory && notify) notify(previousInventory);
}

void CNativeCosmetics::Run(bool enabled, const SimulatedCosmetics::Inventory& inventory, bool expand)
{
    if (!enabled && !expand) { if (m_inventory) Clear(); m_status = "Enable cosmetics to show them in TF2's backpack"; return; }
    if (!Initialize()) return;
    auto native = static_cast<unsigned char*>(manager()) + 408;
    const auto owner = Field<std::uint64_t>(native, 8);
    if (!owner || !Field<void*>(native, 152))
    {
        if (!m_items.empty()) Clear();
        m_status = "Waiting for TF2's Steam inventory";
        return;
    }
    const int capacity = originalCapacity(native);
    const bool nativeChanged = native != m_inventory || owner != m_owner || capacity != m_capacity;
    if (nativeChanged)
    {
        if (!m_items.empty()) Clear();
        m_inventory = native; m_owner = owner; m_capacity = capacity;
    }
    bool changed = nativeChanged;
    if (!enabled)
    {
        if (!m_items.empty()) { Clear(); m_inventory = native; m_owner = owner; m_capacity = capacity; }
        m_status = "TF2 backpack expanded to 4000 slots";
        if (changed) notify(native);
        return;
    }
    for (auto it = m_items.begin(); it != m_items.end();)
    {
        const auto id = it->first & 0x0000FFFFFFFFFFFFULL;
        if (!inventory.Get(id))
        {
            if (find(native, it->first, nullptr)) Remove(native, it->first);
            Destroy(it->second.object);
            it = m_items.erase(it);
            changed = true;
        }
        else ++it;
    }
    for (const auto& item : inventory.items)
    {
        if (!SimulatedCosmetics::Find(F::LocalCosmetics->Catalog(), item.definition)) continue;
        const auto id = SimulatedCosmetics::NativeId(item.id);
        if (!id) continue;
        auto it = m_items.find(id);
        if (it == m_items.end())
        {
            if (find(native, id, nullptr)) continue;
            auto object = create();
            if (!object) continue;
            int position = SimulatedCosmetics::MaxNativeBackpackSlots;
            const auto views = Field<unsigned char*>(native, 96);
            const int count = Field<int>(native, 112);
            if (count < 0 || count > 65535 || (count && !views)) { Destroy(object); continue; }
            for (; position > 0; --position)
            {
                bool occupied = false;
                for (int i = 0; i < count; ++i)
                    if ((Field<std::uint32_t>(views + 344 * i, 108) & 0xffffU) == static_cast<unsigned int>(position)) { occupied = true; break; }
                if (!occupied && std::none_of(m_items.begin(), m_items.end(), [position](const auto& pair) { return pair.second.position == position; })) break;
            }
            if (!position) { Destroy(object); continue; }
            Field<std::uint64_t>(object, 72) = id;
            Field<std::uint32_t>(object, 80) = static_cast<std::uint32_t>(owner);
            Field<std::uint32_t>(object, 84) = static_cast<std::uint32_t>(position);
            Field<std::uint16_t>(object, 88) = static_cast<std::uint16_t>(item.definition);
            Field<std::uint8_t>(object, 90) = 1;
            Field<std::uint8_t>(object, 91) = 6; // Unique
            Field<std::uint8_t>(object, 92) = 3; // Client preview: cannot trade or craft.
            Field<std::uint8_t>(object, 93) = 8; // Found in crate
            Entry entry{object, position, {}};
            entry.equipped.fill(-1);
            it = m_items.emplace(id, entry).first;
            changed = true;
        }
        auto& entry = it->second;
        if (!find(native, id, nullptr))
        {
            // Steam may rebuild its view vector. Reinsert without touching SOCache
            // or acknowledging/delivering an item to the game coordinator.
            Field<std::uint8_t>(entry.object, 95) &= static_cast<std::uint8_t>(~2U);
            if (auto custom = Field<void*>(entry.object, 120)) Field<int>(custom, 72) = 0;
            entry.equipped.fill(-1);
            if (!add(native, entry.object, false, false, false)) continue;
            changed = true;
        }
        std::array<int, 10> equipped;
        equipped.fill(-1);
        for (int c = 1; c <= 9; ++c)
            for (int slot = 0; slot < SimulatedCosmetics::Slots; ++slot)
                if (inventory.equipped[c][slot] == item.id) equipped[c] = SimulatedCosmetics::NativeSlot(slot);
        if (equipped != entry.equipped)
        {
            Field<std::uint8_t>(entry.object, 95) &= static_cast<std::uint8_t>(~2U);
            if (auto custom = Field<void*>(entry.object, 120)) Field<int>(custom, 72) = 0;
            for (int c = 1; c <= 9; ++c)
                if (equipped[c] >= 0) equipObject(entry.object, static_cast<std::uint16_t>(c), static_cast<std::uint16_t>(equipped[c]));
            entry.equipped = equipped;
            changed = true;
        }
    }
    if (changed) notify(native);
    const auto visible = std::count_if(m_items.begin(), m_items.end(), [native](const auto& item) { return find(native, item.first, nullptr) != nullptr; });
    m_status = visible == static_cast<std::ptrdiff_t>(inventory.items.size())
        ? "Simulated cosmetics available in TF2's backpack and loadouts"
        : "Some cosmetics unavailable or the 4000-slot backpack is full";
}

void* CNativeCosmetics::ItemData(void* inventory, std::uint64_t id) const
{
    if (inventory != m_inventory) return nullptr;
    const auto it = m_items.find(id);
    return it == m_items.end() ? nullptr : it->second.object;
}

void* CNativeCosmetics::Loadout(void* inventory, int playerClass, int slot) const
{
    if (inventory != m_inventory || playerClass < 1 || playerClass > 9) return nullptr;
    const int cosmeticSlot = SimulatedCosmetics::CosmeticSlot(slot);
    if (cosmeticSlot < 0) return nullptr;
    const auto id = F::LocalCosmetics->Inventory().equipped[playerClass][cosmeticSlot];
    return id ? find(inventory, SimulatedCosmetics::NativeId(id), nullptr) : nullptr;
}

bool CNativeCosmetics::Equip(void* self, int playerClass, int slot, std::uint64_t id, bool& result)
{
    if (!m_inventory || self != manager()) return false;
    const int cosmeticSlot = SimulatedCosmetics::CosmeticSlot(slot);
    if (auto it = m_items.find(id); it != m_items.end())
    {
        result = cosmeticSlot >= 0 && F::LocalCosmetics->Equip(id & 0x0000FFFFFFFFFFFFULL, playerClass, cosmeticSlot);
        if (result) Run(true, F::LocalCosmetics->Inventory());
        return true; // Never send synthetic item IDs to the coordinator.
    }
    if (playerClass >= 1 && playerClass <= 9 && cosmeticSlot >= 0
        && F::LocalCosmetics->Inventory().equipped[playerClass][cosmeticSlot])
    {
        F::LocalCosmetics->Unequip(playerClass, cosmeticSlot);
        Run(true, F::LocalCosmetics->Inventory());
    }
    return false;
}

int CNativeCosmetics::ExtraCapacity(void* inventory) const
{
    return inventory == m_inventory ? std::max(0, SimulatedCosmetics::MaxNativeBackpackSlots - m_capacity) : 0;
}

void CNativeCosmetics::Shutdown()
{
    if (!m_items.empty()) Clear();
    if (m_ready)
        for (auto target : hookTargets) { MH_DisableHook(target); MH_RemoveHook(target); }
    m_ready = false;
}
