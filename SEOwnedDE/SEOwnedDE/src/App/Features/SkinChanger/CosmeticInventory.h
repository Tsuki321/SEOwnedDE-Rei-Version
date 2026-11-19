#pragma once

// The simulation owns these items; the native backpack bridge reads this state.
// Keep this layer independent of the engine so inventory behavior can be tested.
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace SimulatedCosmetics
{

inline constexpr std::array<const char*, 10> ClassNames = {
    "all", "scout", "sniper", "soldier", "demoman", "medic", "heavy", "pyro", "spy", "engineer"
};
inline constexpr std::size_t MaxItems = 4096;
inline constexpr int Slots = 3;
inline constexpr int MaxNativeBackpackSlots = 4000; // Native backpack panel: 80 pages of 50.
inline constexpr std::uint64_t NativeIdPrefix = 0xF31C000000000000ULL;
inline std::uint64_t NativeId(std::uint64_t id)
{
    return id && id < 0x0001000000000000ULL ? NativeIdPrefix | id : 0;
}
inline int NativeSlot(int slot) { return slot == 0 ? 7 : slot == 1 ? 8 : slot == 2 ? 10 : -1; }
inline int CosmeticSlot(int slot) { return slot == 7 ? 0 : slot == 8 ? 1 : slot == 10 ? 2 : -1; }

struct SchemaNode
{
    std::string value;
    std::map<std::string, SchemaNode> children;

    const SchemaNode& Get(const std::string& key) const
    {
        static const SchemaNode empty;
        const auto it = children.find(key);
        return it == children.end() ? empty : it->second;
    }
    std::string Text(const std::string& key) const { return Get(key).value; }
};

class SchemaParser
{
    std::string_view m_text;
    std::size_t m_pos = 0;

    std::string Token()
    {
        for (;;)
        {
            while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
            if (m_text.substr(m_pos, 2) != "//") break;
            while (m_pos < m_text.size() && m_text[m_pos] != '\n') ++m_pos;
        }
        if (m_pos == m_text.size()) return {};
        const char first = m_text[m_pos++];
        if (first == '{' || first == '}') return std::string(1, first);
        std::string result;
        if (first == '"')
        {
            while (m_pos < m_text.size())
            {
                const char c = m_text[m_pos++];
                if (c == '"') return result;
                if (c == '\\' && m_pos < m_text.size())
                {
                    const char escaped = m_text[m_pos];
                    if (escaped == '"' || escaped == '\\') { result += escaped; ++m_pos; continue; }
                }
                result += c;
            }
            throw std::runtime_error("Unterminated schema string");
        }
        result += first;
        while (m_pos < m_text.size() && !std::isspace(static_cast<unsigned char>(m_text[m_pos]))
            && m_text[m_pos] != '{' && m_text[m_pos] != '}') result += m_text[m_pos++];
        return result;
    }

    SchemaNode Object(int depth, bool nested)
    {
        if (depth > 64) throw std::runtime_error("Schema nesting limit");
        SchemaNode result;
        for (;;)
        {
            const auto key = Token();
            if (key.empty())
            {
                if (nested) throw std::runtime_error("Incomplete schema object");
                return result;
            }
            if (key == "}")
            {
                if (!nested) throw std::runtime_error("Unexpected schema brace");
                return result;
            }
            // Platform condition suffixes do not contain item definitions.
            if (key.front() == '[') continue;
            if (key == "{") throw std::runtime_error("Missing schema key");
            const auto val = Token();
            if (val == "}" || (m_pos == m_text.size() && val.empty()))
                throw std::runtime_error("Missing schema value");
            SchemaNode node;
            if (val == "{") node = Object(depth + 1, true);
            else node.value = val;
            result.children[key] = std::move(node);
        }
    }

public:
    explicit SchemaParser(std::string_view text) : m_text(text)
    {
        if (text.size() > 16 * 1024 * 1024) throw std::runtime_error("Schema size limit");
        if (text.substr(0, 3) == "\xEF\xBB\xBF") m_pos = 3;
    }
    SchemaNode Parse() { return Object(0, false); }
};

inline void Merge(SchemaNode& into, const SchemaNode& from)
{
    if (from.children.empty()) { into = from; return; }
    for (const auto& [key, value] : from.children) Merge(into.children[key], value);
}

inline SchemaNode Resolve(const SchemaNode& item, const SchemaNode& prefabs, int depth = 0)
{
    if (depth > 32) throw std::runtime_error("Cyclic schema prefab");
    SchemaNode result;
    std::istringstream names(item.Text("prefab"));
    std::string name;
    while (names >> name)
    {
        const auto it = prefabs.children.find(name);
        if (it != prefabs.children.end()) Merge(result, Resolve(it->second, prefabs, depth + 1));
    }
    Merge(result, item);
    return result;
}

struct Definition
{
    int index = 0;
    std::string name;
    std::string localization;
    std::array<std::string, 10> models;
    std::vector<std::string> regions;
    std::vector<std::string> conflicts;
    std::map<std::string, int> bodygroups;

    bool Supports(int playerClass) const
    {
        return playerClass > 0 && playerClass < static_cast<int>(models.size()) && !models[playerClass].empty();
    }
};

inline bool ValidModel(std::string_view path)
{
    return path.starts_with("models/") && path.ends_with(".mdl") && path.find("..") == path.npos
        && path.find('\\') == path.npos && path.find(':') == path.npos;
}

inline std::vector<Definition> ReadCatalog(std::string_view schema)
{
    const auto parsed = SchemaParser(schema).Parse();
    const auto& root = parsed.Get("items_game");
    const auto& prefabs = root.Get("prefabs");
    std::vector<Definition> catalog;
    for (const auto& [key, raw] : root.Get("items").children)
    {
        int index = 0;
        const auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), index);
        if (error != std::errc{} || end != key.data() + key.size() || index <= 0 || index > 65535) continue;
        const auto item = Resolve(raw, prefabs);
        const auto slot = item.Text("item_slot");
        if (item.Text("item_class") != "tf_wearable" || (slot != "head" && slot != "misc")) continue;
        if (item.Text("hidden") == "1") continue;
        Definition def;
        def.index = index;
        def.name = item.Text("name");
        def.localization = item.Text("item_name");
        for (int c = 1; c <= 9; ++c)
        {
            const auto used = item.Get("used_by_classes").Text(ClassNames[c]);
            if (used.empty() || used == "0") continue;
            auto model = item.Get("model_player_per_class").Text(ClassNames[c]);
            if (model.empty()) model = item.Text("model_player");
            if (ValidModel(model)) def.models[c] = std::move(model);
        }
        if (std::none_of(def.models.begin(), def.models.end(), [](const auto& m) { return !m.empty(); })) continue;
        if (const auto region = item.Text("equip_region"); !region.empty()) def.regions.push_back(region);
        for (const auto& [region, value] : item.Get("equip_regions").children)
            if (value.value != "0") def.regions.push_back(region);
        def.conflicts = def.regions;
        for (const auto& [group, node] : item.Get("visuals").Get("player_bodygroups").children)
        {
            int value = 0;
            const auto [last, status] = std::from_chars(node.value.data(), node.value.data() + node.value.size(), value);
            if (status == std::errc{} && last == node.value.data() + node.value.size() && value >= 0 && value <= 255)
                def.bodygroups[group] = value;
        }
        for (const auto& region : def.regions)
            for (const auto& [other, value] : root.Get("equip_conflicts").Get(region).children)
                if (value.value != "0") def.conflicts.push_back(other);
        catalog.push_back(std::move(def));
    }
    std::sort(catalog.begin(), catalog.end(), [](const auto& a, const auto& b) { return a.index < b.index; });
    return catalog;
}

inline bool ParseId(std::string_view text, int& id)
{
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
    return error == std::errc{} && end == text.data() + text.size() && id > 0 && id <= 65535;
}

inline const Definition* Find(const std::vector<Definition>& catalog, int index)
{
    const auto it = std::lower_bound(catalog.begin(), catalog.end(), index,
        [](const auto& def, int value) { return def.index < value; });
    return it != catalog.end() && it->index == index ? &*it : nullptr;
}

struct Crate
{
    int definition = 0;
    int series = 0;
    std::string name;
    std::vector<int> contents;
};

inline std::vector<Crate> ReadCrates(std::string_view text, const std::vector<Definition>& catalog)
{
    const auto document = SchemaParser(text).Parse();
    const auto& root = document.Get("items_game");
    const auto& lists = root.Get("client_loot_lists");
    std::map<std::string, int> names;
    for (const auto& [key, node] : root.Get("items").children)
    {
        int id = 0;
        if (ParseId(key, id)) names[node.Text("name")] = id;
    }
    const auto collectItems = [&](auto&& self, const SchemaNode& node, std::vector<int>& into, int depth) -> void
    {
        if (depth > 8) return;
        for (const auto& [key, value] : node.children)
        {
            int id = 0;
            if (!ParseId(key, id))
            {
                const auto found = names.find(key);
                if (found != names.end()) id = found->second;
            }
            if (Find(catalog, id)) into.push_back(id);
            if (!value.children.empty()) self(self, value, into, depth + 1);
        }
    };
    const auto collect = [&](auto&& self, const std::string& name, std::vector<int>& into,
        std::vector<std::string>& path) -> void
    {
        if (path.size() >= 32 || std::find(path.begin(), path.end(), name) != path.end()) return;
        path.push_back(name);
        const auto collection = lists.Get(name).Text("loot_list_collection");
        if (!collection.empty()) collectItems(collectItems, root.Get("item_collections").Get(collection).Get("items"), into, 0);
        for (const auto& [key, node] : lists.Get(name).children)
        {
            int id = 0;
            if (!ParseId(key, id))
            {
                const auto found = names.find(key);
                if (found != names.end()) id = found->second;
            }
            if (Find(catalog, id)) into.push_back(id);
            else if (!lists.Get(key).children.empty()) self(self, key, into, path);
        }
        path.pop_back();
    };
    std::vector<Crate> result;
    for (const auto& [key, node] : root.Get("items").children)
    {
        int id = 0;
        if (!ParseId(key, id)) continue;
        const auto item = Resolve(node, root.Get("prefabs"));
        if (item.Text("item_class") != "supply_crate") continue;
        int series = 0;
        auto seriesText = item.Get("static_attrs").Text("set supply crate series");
        if (seriesText.empty()) seriesText = item.Get("attributes").Get("set supply crate series").Text("value");
        ParseId(seriesText, series);
        std::string list = item.Text("loot_list_name");
        if (list.empty()) list = item.Get("tool").Get("usage").Text("loot_list_name");
        if (list.empty() && series)
            for (const auto& [name, value] : root.Get("revolving_loot_lists").children)
                if (value.value == seriesText) { list = name; break; }
        const auto collection = item.Text("collection_reference");
        if (list.empty() && collection.empty()) continue;
        Crate crate{id, series, item.Text("name"), {}};
        if (crate.name.empty()) crate.name = item.Text("item_name");
        std::vector<std::string> path;
        collect(collect, list, crate.contents, path);
        if (!collection.empty()) collectItems(collectItems, root.Get("item_collections").Get(collection).Get("items"), crate.contents, 0);
        std::sort(crate.contents.begin(), crate.contents.end());
        crate.contents.erase(std::unique(crate.contents.begin(), crate.contents.end()), crate.contents.end());
        if (!crate.contents.empty()) result.push_back(std::move(crate));
    }
    return result;
}

inline bool Conflicts(const Definition& a, const Definition& b)
{
    const auto overlaps = [](const auto& regions, const auto& conflicts)
    {
        for (const auto& region : regions)
            if (std::find(conflicts.begin(), conflicts.end(), region) != conflicts.end()) return true;
        return false;
    };
    return a.index == b.index || overlaps(a.regions, b.conflicts) || overlaps(b.regions, a.conflicts);
}

inline int ApplyBodygroup(int body, int base, int count, int value)
{
    if (body < 0 || base <= 0 || count <= 0 || value < 0 || value >= count) return body;
    const auto result = static_cast<std::int64_t>(body)
        + static_cast<std::int64_t>(value - (body / base) % count) * base;
    return result >= 0 && result <= std::numeric_limits<int>::max() ? static_cast<int>(result) : body;
}

struct Item
{
    std::uint64_t id = 0;
    int definition = 0;
};

class Inventory
{
public:
    std::vector<Item> items;
    std::array<std::array<std::uint64_t, Slots>, 10> equipped = {};
    std::uint64_t nextId = 1;

    const Item* Get(std::uint64_t id) const
    {
        const auto it = std::find_if(items.begin(), items.end(), [id](const auto& item) { return item.id == id; });
        return it == items.end() ? nullptr : &*it;
    }
    std::uint64_t Add(int definition, const std::vector<Definition>& catalog)
    {
        if (items.size() >= MaxItems || !Find(catalog, definition) || !nextId
            || nextId == std::numeric_limits<std::uint64_t>::max()) return 0;
        const auto id = nextId++;
        items.push_back({id, definition});
        return id;
    }
    template<class Generator>
    std::uint64_t OpenCrate(int playerClass, const std::vector<Definition>& catalog, Generator& random)
    {
        if (playerClass < 0 || playerClass > 9 || items.size() >= MaxItems) return 0;
        std::vector<int> pool;
        for (const auto& def : catalog)
            if (!playerClass || def.Supports(playerClass)) pool.push_back(def.index);
        if (pool.empty()) return 0;
        return Add(pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(random)], catalog);
    }
    bool Equip(std::uint64_t id, int playerClass, int slot, const std::vector<Definition>& catalog)
    {
        if (playerClass < 1 || playerClass > 9 || slot < 0 || slot >= Slots) return false;
        const auto* item = Get(id);
        const auto* def = item ? Find(catalog, item->definition) : nullptr;
        if (!def || !def->Supports(playerClass)) return false;
        for (auto& existing : equipped[playerClass])
        {
            const auto* other = Get(existing);
            const auto* otherDef = other ? Find(catalog, other->definition) : nullptr;
            if (existing == id || (otherDef && Conflicts(*def, *otherDef))) existing = 0;
        }
        equipped[playerClass][slot] = id;
        return true;
    }
    bool Remove(std::uint64_t id)
    {
        const auto it = std::find_if(items.begin(), items.end(), [id](const auto& item) { return item.id == id; });
        if (it == items.end()) return false;
        items.erase(it);
        for (auto& loadout : equipped)
            for (auto& equippedId : loadout) if (equippedId == id) equippedId = 0;
        return true;
    }
    void ValidateLoadouts(const std::vector<Definition>& catalog)
    {
        const auto saved = equipped;
        equipped = {};
        for (int c = 1; c <= 9; ++c)
            for (int slot = 0; slot < Slots; ++slot)
                if (saved[c][slot]) Equip(saved[c][slot], c, slot, catalog);
    }
};

}
