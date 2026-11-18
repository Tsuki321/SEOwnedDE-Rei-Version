#include "LocalCosmetics.h"
#include "../CFG.h"
#include "../VisualUtils/VisualUtils.h"
#include "../../../Utils/Storage/Storage.h"
#include <filesystem>
#include <fstream>
#include <format>
#include <iterator>

namespace
{
    // Verified against installed client.dll with IDA MCP (2026-11-18).
    // Resolve lazily: an unsupported client build should only disable previews.
    // The constructor tail disambiguates other 3808-byte entity factories.
    constexpr auto kCreate = "48 83 EC 28 B9 E0 0E 00 00 E8 ? ? ? ? 48 85 C0 74 0C 48 8B C8 48 83 C4 28 E9 C1 FD FF FF";
    constexpr auto kFollow = "48 89 5C 24 08 57 48 83 EC 20 41 0F B6 F8 48 8B D9 48 85 D2 74 5C 45 33 C0 E8 ? ? ? ? 45 33 C0 33 D2";
    constexpr auto kSetModel = "48 89 5C 24 10 57 48 83 EC 20 66 89 91 CC 00 00 00 48 8B D9 48 8B 0D ? ? ? ? 0F BF D2 48 8B 01 FF 50 08";

    struct PreviewFunctions
    {
        using CreateFn = C_BaseAnimating* (__fastcall*)();
        using FollowFn = void (__fastcall*)(C_BaseEntity*, C_BaseEntity*, bool);
        using SetModelFn = void (__fastcall*)(C_BaseEntity*, int);
        CreateFn create = reinterpret_cast<CreateFn>(Memory::FindSignature("client.dll", kCreate));
        FollowFn follow = reinterpret_cast<FollowFn>(Memory::FindSignature("client.dll", kFollow));
        SetModelFn setModel = reinterpret_cast<SetModelFn>(Memory::FindSignature("client.dll", kSetModel));
    };

    std::filesystem::path InventoryPath() { return U::Storage->GetConfigFolder() / "simulated_cosmetics.json"; }

    std::string ReadFile(const std::filesystem::path& path, std::size_t limit)
    {
        if (std::filesystem::file_size(path) > limit) throw std::runtime_error("File size limit");
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open file");
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
}

void CLocalCosmetics::Load()
{
    try
    {
        if (std::filesystem::exists(InventoryPath()))
            m_state = SimulatedCosmetics::Decode(nlohmann::json::parse(ReadFile(InventoryPath(), 1024 * 1024)));
        m_loadFailed = false;
        m_status = "Local inventory loaded";
    }
    catch (const std::exception&)
    {
        // Keep the original file intact when parsing fails.
        m_loadFailed = true;
        m_status = "Inventory load failed; original file kept";
    }
    RefreshCatalog();
}

bool CLocalCosmetics::Save()
{
    if (m_loadFailed) { m_status = "Fix inventory file, then Reload"; return false; }
    const auto path = InventoryPath();
    auto temporary = path;
    temporary += ".tmp";
    try
    {
        std::filesystem::create_directories(path.parent_path());
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file) throw std::runtime_error("Cannot save inventory");
            file << SimulatedCosmetics::Encode(m_state).dump(2);
            file.flush();
            if (!file) throw std::runtime_error("Cannot write inventory");
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace inventory");
        return true;
    }
    catch (const std::exception&)
    {
        m_status = "Save failed; changes remain in memory";
        return false;
    }
}

void CLocalCosmetics::RefreshCatalog()
{
    try
    {
        const auto path = std::filesystem::path(I::EngineClient->GetGameDirectory()) / "scripts/items/items_game.txt";
        auto catalog = SimulatedCosmetics::ReadCatalog(ReadFile(path, 16 * 1024 * 1024));
        if (catalog.empty()) throw std::runtime_error("Empty cosmetic catalog");
        m_catalog.swap(catalog);
        m_refreshRuntime = true;
        m_state.inventory.ValidateLoadouts(m_catalog);
        if (!m_loadFailed) m_status = std::format("{} cosmetic definitions ready", m_catalog.size());
    }
    catch (const std::exception&) { m_status = "Catalog unavailable; check TF2 item schema"; }
}

std::string CLocalCosmetics::Name(int definition) const
{
    const auto* def = SimulatedCosmetics::Find(m_catalog, definition);
    return def && !def->name.empty() ? def->name : std::format("Item {}", definition);
}

void CLocalCosmetics::SetEnabled(bool enabled) { m_state.enabled = enabled; Save(); }
void CLocalCosmetics::SetHideOriginals(bool hide) { m_state.hideOriginals = hide; Save(); }

std::uint64_t CLocalCosmetics::OpenCrate(int playerClass)
{
    if (m_loadFailed) { m_status = "Reload a valid inventory first"; return 0; }
    const auto id = m_state.inventory.OpenCrate(playerClass, m_catalog, m_random);
    if (!id) { m_status = "Crate empty or inventory full"; return 0; }
    m_status = "Unboxed: " + Name(m_state.inventory.Get(id)->definition);
    Save();
    return id;
}

bool CLocalCosmetics::Equip(std::uint64_t id, int playerClass, int slot)
{
    if (!m_state.inventory.Equip(id, playerClass, slot, m_catalog))
    { m_status = "Cosmetic does not fit this class"; return false; }
    m_status = "Equipped locally";
    Save();
    return true;
}

void CLocalCosmetics::Unequip(int playerClass, int slot)
{
    if (playerClass < 1 || playerClass > 9 || slot < 0 || slot >= SimulatedCosmetics::Slots) return;
    m_state.inventory.equipped[playerClass][slot] = 0;
    Save();
}

void CLocalCosmetics::Remove(std::uint64_t id) { if (m_state.inventory.Remove(id)) Save(); }

void CLocalCosmetics::ReleaseSlot(RuntimeSlot& slot)
{
    if (I::ClientEntityList && slot.entity.IsValid())
        if (const auto entity = I::ClientEntityList->GetClientEntityFromHandle(slot.entity)) entity->Release();
    if (I::ModelInfoClient && slot.model < -1) I::ModelInfoClient->ReleaseDynamicModel(slot.model);
    slot = {};
}

void CLocalCosmetics::ResetRuntime()
{
    for (auto& slot : m_runtime) ReleaseSlot(slot);
    m_owner = {};
    m_class = 0;
    m_team = 0;
}

void CLocalCosmetics::LevelShutdown()
{
    ResetRuntime();
    if (m_shutdownRequested.load()) m_shutdownDone.store(true);
}

void CLocalCosmetics::Shutdown()
{
    // Entity and leaf-system changes must happen on the client frame thread.
    m_shutdownRequested.store(true);
    // Do not detach the hooks while client entities are still being released.
    while (m_frameSeen.load() && !m_shutdownDone.load()) Sleep(10);
    Save();
}

void CLocalCosmetics::Run()
{
    m_frameSeen.store(true);
    if (m_shutdownRequested.load())
    {
        ResetRuntime();
        m_shutdownDone.store(true);
        return;
    }
    if (m_refreshRuntime) { ResetRuntime(); m_refreshRuntime = false; }
    const auto local = H::Entities->GetLocal();
    const bool screenshot = CFG::Misc_Clean_Screenshot && F::VisualUtils->IsTakingScreenshotCached();
    if (!m_state.enabled || !local || local->deadflag() || local->m_flInvisibility() > 0.0f
        || local->InCond(TF_COND_DISGUISED) || screenshot || !I::EngineClient->IsInGame())
    { ResetRuntime(); m_runtimeStatus = "Preview inactive"; return; }
    const int playerClass = local->m_iClass();
    if (playerClass < 1 || playerClass > 9) { ResetRuntime(); return; }
    if (m_owner != local->GetRefEHandle() || m_class != playerClass || m_team != local->m_iTeamNum()) ResetRuntime();
    m_owner = local->GetRefEHandle();
    m_class = playerClass;
    m_team = local->m_iTeamNum();
    static const PreviewFunctions functions;
    if (!functions.create || !functions.follow || !functions.setModel)
    { m_runtimeStatus = "Client build unsupported for previews"; return; }

    bool loading = false;
    bool failed = false;
    for (int i = 0; i < SimulatedCosmetics::Slots; ++i)
    {
        auto& slot = m_runtime[i];
        const auto id = m_state.inventory.equipped[playerClass][i];
        if (slot.item != id) ReleaseSlot(slot);
        if (!id) continue;
        const auto* item = m_state.inventory.Get(id);
        const auto* def = item ? SimulatedCosmetics::Find(m_catalog, item->definition) : nullptr;
        if (!def || !def->Supports(playerClass)) { ReleaseSlot(slot); continue; }
        if (!slot.item)
        {
            slot.item = id;
            slot.model = I::ModelInfoClient->RegisterDynamicModel(def->models[playerClass].c_str(), true);
            if (slot.model < -1) I::ModelInfoClient->AddRefDynamicModel(slot.model);
        }
        if (slot.model == -1) { failed = true; continue; }
        if (I::ModelInfoClient->IsDynamicModelLoading(slot.model)) { loading = true; continue; }
        if (slot.entity.IsValid())
        {
            if (const auto entity = I::ClientEntityList->GetClientEntityFromHandle(slot.entity))
            {
                entity->As<C_BaseAnimating>()->m_nSkin() = m_team == TF_TEAM_BLUE ? 1 : 0;
                continue;
            }
            slot.entity = {};
            slot.attempted = false;
        }
        if (slot.attempted) { failed = true; continue; }
        slot.attempted = true;
        const auto wearable = functions.create();
        if (!wearable) { failed = true; continue; }
        // Item definition is a uint16 in CEconItemView, not a weapon-sized int.
        const int itemOffset = NetVars::GetNetVar("CEconEntity", "m_iItemDefinitionIndex");
        if (itemOffset <= 0) { wearable->Release(); failed = true; continue; }
        *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uintptr_t>(wearable) + itemOffset) = static_cast<std::uint16_t>(def->index);
        const int initializedOffset = NetVars::GetNetVar("CEconEntity", "m_bInitialized");
        if (initializedOffset <= 0) { wearable->Release(); failed = true; continue; }
        *reinterpret_cast<bool*>(reinterpret_cast<std::uintptr_t>(wearable) + initializedOffset) = true;
        wearable->m_hOwnerEntity() = local->GetRefEHandle();
        wearable->m_iTeamNum() = m_team;
        using InitializeFn = bool (__fastcall*)(C_BaseEntity*, const char*, int);
        // C_EconEntity's override sets its client-only flag before calling the base.
        if (!reinterpret_cast<InitializeFn>(Memory::GetVFunc(wearable, 82))(wearable, nullptr, 7))
        { wearable->Release(); failed = true; continue; }
        functions.setModel(wearable, slot.model);
        wearable->m_nSkin() = m_team == TF_TEAM_BLUE ? 1 : 0;
        functions.follow(wearable, local, true);
        wearable->m_fEffects() |= EF_BONEMERGE_FASTCULL | EF_PARENT_ANIMATES;
        slot.entity = wearable->GetRefEHandle();
    }
    m_runtimeStatus = failed ? "A cosmetic model could not load" : loading ? "Loading cosmetic models" : "Visible on your character in third person";
}

bool CLocalCosmetics::ShouldHideOriginal(C_BaseEntity* entity) const
{
    if (!m_state.enabled || !m_state.hideOriginals || !entity || m_shutdownRequested.load()) return false;
    if (CFG::Misc_Clean_Screenshot && F::VisualUtils->IsTakingScreenshotCached()) return false;
    const auto local = H::Entities->GetLocal();
    if (!local || local->deadflag() || m_owner != local->GetRefEHandle()) return false;
    if (entity->GetClassId() != ETFClassIds::CTFWearable || entity->m_hOwnerEntity() != m_owner) return false;
    return std::any_of(m_runtime.begin(), m_runtime.end(), [](const auto& slot) { return slot.entity.IsValid(); });
}

bool CLocalCosmetics::IsPreviewRenderable(IClientRenderable* renderable) const
{
    if (!renderable) return false;
    for (const auto& slot : m_runtime)
        if (slot.entity.IsValid())
            if (const auto entity = I::ClientEntityList->GetClientEntityFromHandle(slot.entity))
                if (entity->GetClientRenderable() == renderable) return true;
    return false;
}

int CLocalCosmetics::PlayerBody(const ModelRenderInfo_t& info) const
{
    if (!m_state.enabled || !info.pModel || m_shutdownRequested.load()) return info.body;
    if (CFG::Misc_Clean_Screenshot && F::VisualUtils->IsTakingScreenshotCached()) return info.body;
    const auto local = H::Entities->GetLocal();
    if (!local || local->deadflag() || info.entity_index != local->entindex()
        || m_owner != local->GetRefEHandle() || m_class < 1 || m_class > 9) return info.body;
    const auto studio = I::ModelInfoClient->GetStudiomodel(info.pModel);
    if (!studio || studio->numbodyparts < 0 || studio->numbodyparts > 128) return info.body;
    int body = info.body;
    for (const auto& slot : m_runtime)
    {
        if (!slot.entity.IsValid() || !I::ClientEntityList->GetClientEntityFromHandle(slot.entity)) continue;
        const auto item = m_state.inventory.Get(slot.item);
        const auto def = item ? SimulatedCosmetics::Find(m_catalog, item->definition) : nullptr;
        if (!def) continue;
        for (int i = 0; i < studio->numbodyparts; ++i)
        {
            const auto part = studio->pBodypart(i);
            const auto replacement = def->bodygroups.find(part->pszName());
            if (replacement != def->bodygroups.end())
                body = SimulatedCosmetics::ApplyBodygroup(body, part->base, part->nummodels, replacement->second);
        }
    }
    return body;
}
