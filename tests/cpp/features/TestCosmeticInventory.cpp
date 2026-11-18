#include <gtest/gtest.h>
#include "App/Features/SkinChanger/CosmeticInventoryPersistence.h"
#include <set>

using namespace SimulatedCosmetics;

namespace
{
constexpr auto schema = R"VDF(
// Minimal schema with prefab inheritance, class models and equip conflicts.
"items_game" {
 "prefabs" {
  "wearable" { "item_class" "tf_wearable" "item_slot" "head" "hidden" "0" }
  "hat" { "prefab" "wearable" "equip_region" "hat" }
 }
 "equip_conflicts" { "whole_head" { "hat" "1" "glasses" "1" } }
 "items" {
  "101" { "prefab" "hat" "name" "Scout Hat" "model_player" "models/scout_hat.mdl"
           "used_by_classes" { "scout" "1" "sniper" "0" } }
  "102" { "prefab" "wearable" "name" "Glasses" "item_slot" "misc" "equip_region" "glasses"
           "model_player" "models/glasses.mdl" "used_by_classes" { "scout" "1" "sniper" "1" }
           "model_player_per_class" { "sniper" "models/sniper_glasses.mdl" } }
  "103" { "prefab" "hat" "name" "Helmet" "equip_region" "whole_head"
           "model_player" "models/helmet.mdl" "used_by_classes" { "scout" "1" } }
  "104" { "prefab" "hat" "name" "Hidden" "hidden" "1"
           "model_player" "models/hidden.mdl" "used_by_classes" { "scout" "1" } }
  "105" { "prefab" "hat" "name" "Unsafe model" "model_player" "models/../external.mdl"
           "used_by_classes" { "scout" "1" } }
  "106" { "prefab" "wearable" "item_class" "tf_weapon_scattergun" "item_slot" "primary"
           "model_player" "models/weapon.mdl" "used_by_classes" { "scout" "1" } }
 }
}
)VDF";
}

TEST(CosmeticSchema, ResolvesPrefabsAndClassModelsWithoutWeaponOrHiddenItems)
{
    const auto catalog = ReadCatalog(schema);
    ASSERT_EQ(catalog.size(), 3u);
    EXPECT_EQ(catalog[0].index, 101);
    EXPECT_TRUE(catalog[0].Supports(1));
    EXPECT_FALSE(catalog[0].Supports(2));
    EXPECT_EQ(catalog[1].models[1], "models/glasses.mdl");
    EXPECT_EQ(catalog[1].models[2], "models/sniper_glasses.mdl");
    EXPECT_FALSE(catalog[0].Supports(-1));
    EXPECT_FALSE(catalog[0].Supports(10));
}

TEST(CosmeticSchema, SupportsCommentsEscapesAndPlatformConditions)
{
    const auto root = SchemaParser(R"VDF("root" { "name" "Hat \"One\"" [$WIN32] // comment
      "value" "" "next" "yes" })VDF").Parse();
    EXPECT_EQ(root.Get("root").Text("name"), "Hat \"One\"");
    EXPECT_EQ(root.Get("root").Text("next"), "yes");
}

TEST(CosmeticSchema, RejectsIncompleteAndCyclicSchemas)
{
    EXPECT_THROW(SchemaParser("\"root\" { \"name\" \"bad").Parse(), std::runtime_error);
    EXPECT_THROW(SchemaParser("\"root\" {").Parse(), std::runtime_error);
    EXPECT_THROW(SchemaParser("}").Parse(), std::runtime_error);
    const auto cycle = R"("items_game" { "prefabs" { "a" { "prefab" "b" } "b" { "prefab" "a" } }
        "items" { "1" { "prefab" "a" } } })";
    EXPECT_THROW(ReadCatalog(cycle), std::runtime_error);
}

TEST(CosmeticSchema, RejectsModelPathsOutsideModelDirectory)
{
    EXPECT_TRUE(ValidModel("models/player/items/test.mdl"));
    EXPECT_FALSE(ValidModel("../models/test.mdl"));
    EXPECT_FALSE(ValidModel("models/../test.mdl"));
    EXPECT_FALSE(ValidModel("models\\test.mdl"));
    EXPECT_FALSE(ValidModel("models/test.vmt"));
    EXPECT_FALSE(ValidModel("C:/models/test.mdl"));
}

TEST(CosmeticCrate, RestrictsClassPoolAndCreatesDistinctInstances)
{
    const auto catalog = ReadCatalog(schema);
    Inventory inventory;
    std::mt19937_64 random(1234);
    const auto first = inventory.OpenCrate(2, catalog, random);
    const auto second = inventory.OpenCrate(2, catalog, random);
    ASSERT_NE(first, 0u);
    ASSERT_NE(second, first);
    EXPECT_EQ(inventory.Get(first)->definition, 102);
    EXPECT_EQ(inventory.Get(second)->definition, 102);
    EXPECT_EQ(inventory.OpenCrate(9, catalog, random), 0u);
    EXPECT_EQ(inventory.OpenCrate(-1, catalog, random), 0u);
    EXPECT_EQ(inventory.OpenCrate(10, catalog, random), 0u);
    EXPECT_EQ(inventory.items.size(), 2u);
}

TEST(CosmeticCrate, AllCosmeticsPoolCanDrawEveryDefinition)
{
    const auto catalog = ReadCatalog(schema);
    Inventory inventory;
    std::mt19937_64 random(5678);
    std::set<int> drawn;
    for (int i = 0; i < 100; ++i)
    {
        const auto id = inventory.OpenCrate(0, catalog, random);
        ASSERT_NE(id, 0u);
        drawn.insert(inventory.Get(id)->definition);
    }
    EXPECT_EQ(drawn, (std::set<int>{101, 102, 103}));
}

TEST(CosmeticInventory, EquipsPerClassAndClearsConflictingRegions)
{
    const auto catalog = ReadCatalog(schema);
    Inventory inventory;
    const auto hat = inventory.Add(101, catalog);
    const auto glasses = inventory.Add(102, catalog);
    const auto helmet = inventory.Add(103, catalog);
    ASSERT_TRUE(inventory.Equip(hat, 1, 0, catalog));
    ASSERT_TRUE(inventory.Equip(glasses, 1, 1, catalog));
    ASSERT_TRUE(inventory.Equip(glasses, 2, 2, catalog));
    ASSERT_TRUE(inventory.Equip(helmet, 1, 2, catalog));
    EXPECT_EQ(inventory.equipped[1], (std::array<std::uint64_t, Slots>{0, 0, helmet}));
    EXPECT_EQ(inventory.equipped[2][2], glasses);
    EXPECT_FALSE(inventory.Equip(hat, 2, 0, catalog));
    EXPECT_FALSE(inventory.Equip(999, 1, 0, catalog));
    EXPECT_FALSE(inventory.Equip(hat, 0, 0, catalog));
    EXPECT_FALSE(inventory.Equip(hat, 1, 3, catalog));
}

TEST(CosmeticInventory, MovingAndRemovingItemsCleansEverySlot)
{
    const auto catalog = ReadCatalog(schema);
    Inventory inventory;
    const auto glasses = inventory.Add(102, catalog);
    ASSERT_TRUE(inventory.Equip(glasses, 1, 0, catalog));
    ASSERT_TRUE(inventory.Equip(glasses, 1, 2, catalog));
    EXPECT_EQ(inventory.equipped[1][0], 0u);
    ASSERT_TRUE(inventory.Equip(glasses, 2, 1, catalog));
    ASSERT_TRUE(inventory.Remove(glasses));
    EXPECT_EQ(inventory.equipped[1][2], 0u);
    EXPECT_EQ(inventory.equipped[2][1], 0u);
    EXPECT_EQ(inventory.Add(102, catalog), glasses + 1);
    EXPECT_FALSE(inventory.Remove(glasses));
}

TEST(CosmeticInventory, StopsAtCapacityWithoutAdvancingId)
{
    const auto catalog = ReadCatalog(schema);
    Inventory inventory;
    for (std::size_t i = 0; i < MaxItems; ++i) ASSERT_NE(inventory.Add(101, catalog), 0u);
    const auto next = inventory.nextId;
    EXPECT_EQ(inventory.Add(101, catalog), 0u);
    EXPECT_EQ(inventory.nextId, next);
    EXPECT_EQ(inventory.items.size(), MaxItems);
}

TEST(CosmeticInventoryPersistence, RoundTripsItemsAndPerClassLoadouts)
{
    const auto catalog = ReadCatalog(schema);
    SavedState original;
    original.enabled = true;
    original.hideOriginals = true;
    const auto id = original.inventory.Add(102, catalog);
    ASSERT_TRUE(original.inventory.Equip(id, 1, 0, catalog));
    ASSERT_TRUE(original.inventory.Equip(id, 2, 2, catalog));
    const auto loaded = Decode(nlohmann::json::parse(Encode(original).dump()));
    EXPECT_TRUE(loaded.enabled);
    EXPECT_TRUE(loaded.hideOriginals);
    ASSERT_EQ(loaded.inventory.items.size(), 1u);
    EXPECT_EQ(loaded.inventory.items[0].id, id);
    EXPECT_EQ(loaded.inventory.items[0].definition, 102);
    EXPECT_EQ(loaded.inventory.nextId, original.inventory.nextId);
    EXPECT_EQ(loaded.inventory.equipped, original.inventory.equipped);
}

TEST(CosmeticInventoryPersistence, RejectsDuplicateNegativeOverflowAndMissingIds)
{
    SavedState original;
    original.inventory.Add(101, ReadCatalog(schema));
    const auto valid = Encode(original);
    auto invalid = valid;
    invalid["items"].push_back(invalid["items"][0]);
    EXPECT_THROW(Decode(invalid), std::runtime_error);
    invalid = valid;
    invalid["items"][0]["id"] = -1;
    EXPECT_THROW(Decode(invalid), std::runtime_error);
    invalid = valid;
    invalid["items"][0]["id"] = std::numeric_limits<std::uint64_t>::max();
    EXPECT_THROW(Decode(invalid), std::runtime_error);
    invalid = valid;
    invalid["next_id"] = 1;
    EXPECT_THROW(Decode(invalid), std::runtime_error);
    invalid = valid;
    invalid["equipped"][1][0] = 999;
    EXPECT_THROW(Decode(invalid), std::runtime_error);
}

TEST(CosmeticInventoryPersistence, ClearsStaleLoadoutsWhileKeepingOwnedInstances)
{
    const auto catalog = ReadCatalog(schema);
    Inventory inventory;
    const auto hat = inventory.Add(101, catalog);
    const auto glasses = inventory.Add(102, catalog);
    inventory.equipped[1] = {hat, glasses, hat};
    inventory.equipped[2] = {hat, 999, glasses};
    inventory.ValidateLoadouts(catalog);
    EXPECT_EQ(inventory.equipped[1], (std::array<std::uint64_t, Slots>{0, glasses, hat}));
    EXPECT_EQ(inventory.equipped[2], (std::array<std::uint64_t, Slots>{0, 0, glasses}));
    EXPECT_EQ(inventory.items.size(), 2u);
}
