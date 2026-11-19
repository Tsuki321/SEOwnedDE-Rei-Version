#pragma once
#include "CosmeticInventory.h"
#include <nlohmann/json.hpp>
#include <set>

namespace SimulatedCosmetics
{
struct SavedState
{
    Inventory inventory;
    bool enabled = false;
    bool hideOriginals = false;
    bool expandBackpack = false;
};

inline nlohmann::json Encode(const SavedState& state)
{
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& item : state.inventory.items)
        entries.push_back({{"id", item.id}, {"definition", item.definition}});
    return {{"version", 1}, {"enabled", state.enabled}, {"hide_originals", state.hideOriginals}, {"expand_backpack", state.expandBackpack},
        {"next_id", state.inventory.nextId}, {"items", entries}, {"equipped", state.inventory.equipped}};
}

inline std::uint64_t ReadId(const nlohmann::json& value, bool allowZero = false)
{
    if (!value.is_number_unsigned() && (!value.is_number_integer() || value.get<std::int64_t>() < 0))
        throw std::runtime_error("Invalid simulated item ID");
    const auto id = value.get<std::uint64_t>();
    if ((!id && !allowZero) || id == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Simulated item ID outside range");
    return id;
}

inline SavedState Decode(const nlohmann::json& json)
{
    if (!json.is_object() || json.at("version") != 1 || !json.at("items").is_array()
        || json.at("items").size() > MaxItems || !json.at("enabled").is_boolean()
        || !json.at("hide_originals").is_boolean()) throw std::runtime_error("Invalid simulated inventory");
    SavedState result;
    result.enabled = json.at("enabled").get<bool>();
    result.hideOriginals = json.at("hide_originals").get<bool>();
    if (json.contains("expand_backpack"))
    {
        if (!json.at("expand_backpack").is_boolean()) throw std::runtime_error("Invalid backpack expansion setting");
        result.expandBackpack = json.at("expand_backpack").get<bool>();
    }
    std::set<std::uint64_t> ids;
    std::uint64_t largest = 0;
    for (const auto& entry : json.at("items"))
    {
        const auto id = ReadId(entry.at("id"));
        const auto& def = entry.at("definition");
        if (!def.is_number_integer() || def < 1 || def > 65535 || !ids.insert(id).second)
            throw std::runtime_error("Invalid simulated item definition or duplicate ID");
        result.inventory.items.push_back({id, def.get<int>()});
        largest = std::max(largest, id);
    }
    result.inventory.nextId = ReadId(json.at("next_id"));
    if (result.inventory.nextId <= largest) throw std::runtime_error("Invalid next simulated item ID");
    const auto& equipped = json.at("equipped");
    if (!equipped.is_array() || equipped.size() != 10) throw std::runtime_error("Invalid cosmetic loadouts");
    for (int c = 0; c <= 9; ++c)
    {
        if (!equipped[c].is_array() || equipped[c].size() != Slots) throw std::runtime_error("Invalid cosmetic slots");
        for (int slot = 0; slot < Slots; ++slot)
        {
            const auto id = ReadId(equipped[c][slot], true);
            if (id && (!c || !ids.contains(id))) throw std::runtime_error("Missing equipped simulated item");
            result.inventory.equipped[c][slot] = id;
        }
    }
    return result;
}
}
