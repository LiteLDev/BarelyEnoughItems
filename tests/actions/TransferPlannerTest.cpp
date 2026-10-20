// TransferPlanner is game-independent but lives in src/actions; compile it into the test binary
// directly (bei-tests links only bei-core).
#include "actions/TransferPlanner.cpp"

#include <gtest/gtest.h>

using namespace bei::actions;
using namespace bei::core;

namespace {
IngredientView any(char const* id) { return {{ItemKey{id, ItemKey::ANY_AUX}}, 1}; }
IngredientView exact(char const* id, std::int16_t aux = 0) { return {{ItemKey{id, aux}}, 1}; }

RecipeView shaped(int w, int h, std::vector<IngredientView> inputs) {
    RecipeView r;
    r.category = category::CRAFTING;
    r.width    = w;
    r.height   = h;
    r.inputs   = std::move(inputs);
    return r;
}

RecipeView shapeless(std::vector<IngredientView> inputs) {
    RecipeView r;
    r.category  = category::CRAFTING;
    r.shapeless = true;
    r.inputs    = std::move(inputs);
    return r;
}
} // namespace

TEST(TransferPlanner, ShapedKeepsShapeInLargerGrid) {
    auto r = shaped(2, 2, {any("a"), any("b"), any("c"), any("d")});
    auto m = mapToGrid(r, 3);
    ASSERT_TRUE(m.fits);
    EXPECT_EQ(m.cellOfInput, (std::vector<int>{0, 1, 3, 4}));
    auto m2 = mapToGrid(r, 2);
    ASSERT_TRUE(m2.fits);
    EXPECT_EQ(m2.cellOfInput, (std::vector<int>{0, 1, 2, 3}));
}

TEST(TransferPlanner, ShapedTrimsEmptyBorder) {
    // A 3x3 recipe whose only inputs sit in the bottom-right 2x1 area fits the 2x2 grid.
    std::vector<IngredientView> in(9);
    in[5]  = any("a");
    in[8]  = any("a");
    auto m = mapToGrid(shaped(3, 3, in), 2);
    ASSERT_TRUE(m.fits);
    EXPECT_EQ(m.cellOfInput[5], 0);
    EXPECT_EQ(m.cellOfInput[8], 2);
    EXPECT_EQ(m.cellOfInput[0], -1);
}

TEST(TransferPlanner, TooLargeDoesNotFit) {
    std::vector<IngredientView> ring(9, any("minecraft:oak_planks"));
    ring[4] = {};
    EXPECT_FALSE(mapToGrid(shaped(3, 3, ring), 2).fits);
    EXPECT_TRUE(mapToGrid(shaped(3, 3, ring), 3).fits);
    EXPECT_FALSE(mapToGrid(shaped(3, 3, ring), 0).fits);
}

TEST(TransferPlanner, ShapelessFillsRowByRow) {
    auto r = shapeless({any("a"), {}, any("b"), any("c")});
    auto m = mapToGrid(r, 2);
    ASSERT_TRUE(m.fits);
    EXPECT_EQ(m.cellOfInput, (std::vector<int>{0, -1, 1, 2}));
    EXPECT_FALSE(mapToGrid(shapeless(std::vector<IngredientView>(5, any("a"))), 2).fits);
}

TEST(TransferPlanner, CountsAreShared) {
    // 4 planks needed, only 3 owned: exactly one input is missing.
    auto                   r = shaped(2, 2, std::vector<IngredientView>(4, any("minecraft:oak_planks")));
    std::vector<PoolEntry> pool;
    addToPool(pool, {"minecraft:oak_planks", 0}, 2, 64);
    addToPool(pool, {"minecraft:oak_planks", 0}, 1, 64);
    auto a = assign(r, pool, 1);
    EXPECT_EQ(a.missing.size(), 1u);
    addToPool(pool, {"minecraft:oak_planks", 0}, 1, 64);
    EXPECT_TRUE(assign(r, pool, 1).complete());
}

TEST(TransferPlanner, WildcardMatchesAnyAuxButOneKindPerCell) {
    auto                   r = shapeless({any("minecraft:wool")});
    std::vector<PoolEntry> pool;
    addToPool(pool, {"minecraft:wool", 3}, 1, 64);
    addToPool(pool, {"minecraft:wool", 14}, 2, 64);
    auto a = assign(r, pool, 2);
    ASSERT_TRUE(a.complete());
    EXPECT_EQ(a.chosen[0], (ItemKey{"minecraft:wool", 14}));
    EXPECT_FALSE(assign(r, pool, 3).complete()); // 3 total, but no single kind has 3
    EXPECT_FALSE(assign(shapeless({exact("minecraft:wool", 5)}), pool, 1).complete());
}

TEST(TransferPlanner, ConstrainedInputsFirst) {
    // Input 0 accepts oak or birch, input 1 only oak. With one of each, both must be satisfied.
    RecipeView             r = shapeless({
        {{ItemKey{"oak", 0}, ItemKey{"birch", 0}}, 1},
        exact("oak")
    });
    std::vector<PoolEntry> pool;
    addToPool(pool, {"oak", 0}, 1, 64);
    addToPool(pool, {"birch", 0}, 1, 64);
    auto a = assign(r, pool, 1);
    ASSERT_TRUE(a.complete());
    EXPECT_EQ(a.chosen[0], (ItemKey{"birch", 0}));
    EXPECT_EQ(a.chosen[1], (ItemKey{"oak", 0}));
}

TEST(TransferPlanner, MaxSetsBoundedByItemsAndStackSize) {
    auto                   r = shapeless({any("log")});
    std::vector<PoolEntry> pool;
    addToPool(pool, {"log", 0}, 5, 64);
    EXPECT_EQ(maxSets(r, pool), 5);
    auto two = shaped(1, 2, {any("plank"), any("plank")});
    pool.clear();
    addToPool(pool, {"plank", 0}, 9, 64);
    EXPECT_EQ(maxSets(two, pool), 4);
    pool.clear();
    addToPool(pool, {"pearl", 0}, 40, 16);
    EXPECT_EQ(maxSets(shapeless({any("pearl")}), pool), 16);
    pool.clear();
    EXPECT_EQ(maxSets(r, pool), 0);
}

TEST(TransferPlanner, StationForCategory) {
    EXPECT_EQ(stationFor(category::CRAFTING), Station::Crafting);
    EXPECT_EQ(stationFor(category::FURNACE), Station::Furnace);
    EXPECT_EQ(stationFor(category::BLAST_FURNACE), Station::BlastFurnace);
    EXPECT_EQ(stationFor(category::SMOKER), Station::Smoker);
    EXPECT_EQ(stationFor(category::SMITHING), Station::SmithingTable);
    EXPECT_EQ(stationFor(category::CAMPFIRE), Station::None);
    EXPECT_EQ(stationFor(category::BREWING), Station::None);
    EXPECT_EQ(stationFor(category::FUEL), Station::None);
    EXPECT_EQ(stationFor(category::ANVIL), Station::None);
    EXPECT_EQ(stationFor(category::COMPOSTING), Station::None);
    EXPECT_EQ(stationFor("bei:tag/custom"), Station::None);
}

TEST(TransferPlanner, FurnaceTagsRoundTrip) {
    for (auto s : {Station::Furnace, Station::BlastFurnace, Station::Smoker}) {
        EXPECT_FALSE(furnaceTagOf(s).empty());
        EXPECT_EQ(furnaceStationOf(furnaceTagOf(s)), s);
    }
    EXPECT_EQ(furnaceTagOf(Station::Crafting), "");
    EXPECT_EQ(furnaceTagOf(Station::SmithingTable), "");
    EXPECT_EQ(furnaceStationOf("campfire"), Station::None);
    EXPECT_EQ(furnaceStationOf(""), Station::None);
}

TEST(TransferPlanner, PickFuelFollowsPreferenceAndSkipsIngredient) {
    std::vector<ItemKey>   fuels{{"coal", 0}, {"charcoal", 0}, {"log", ItemKey::ANY_AUX}};
    std::vector<PoolEntry> pool;
    addToPool(pool, {"log", 2}, 10, 64);
    addToPool(pool, {"charcoal", 0}, 3, 64);
    // Coal is preferred but missing: charcoal (before logs, by preference order).
    EXPECT_EQ(pickFuel(fuels, pool, {"iron_ore", 0}), (ItemKey{"charcoal", 0}));
    // A wildcard fuel matches a concrete aux; the ingredient itself is never used as fuel.
    pool.clear();
    addToPool(pool, {"log", 2}, 10, 64);
    EXPECT_EQ(pickFuel(fuels, pool, {"iron_ore", 0}), (ItemKey{"log", 2}));
    EXPECT_TRUE(pickFuel(fuels, pool, {"log", 2}).empty());
    // Another log variant still burns while smelting this one.
    addToPool(pool, {"log", 1}, 4, 64);
    EXPECT_EQ(pickFuel(fuels, pool, {"log", 2}), (ItemKey{"log", 1}));
    EXPECT_TRUE(pickFuel({}, pool, {}).empty());
}

TEST(TransferPlanner, SmeltingAndSmithingSets) {
    // Smelting: one input of count 1; shift moves up to a full stack.
    RecipeView smelt;
    smelt.category = category::FURNACE;
    smelt.inputs   = {any("raw_iron")};
    std::vector<PoolEntry> pool;
    addToPool(pool, {"raw_iron", 0}, 100, 64);
    EXPECT_EQ(maxSets(smelt, pool), 64);
    EXPECT_TRUE(assign(smelt, pool, 1).complete());

    // Smithing: template, base, addition; an unstackable base allows one set.
    RecipeView smith;
    smith.category = category::SMITHING;
    smith.inputs   = {exact("upgrade"), any("diamond_sword"), exact("netherite_ingot")};
    pool.clear();
    addToPool(pool, {"upgrade", 0}, 3, 64);
    addToPool(pool, {"diamond_sword", 0}, 2, 1);
    addToPool(pool, {"netherite_ingot", 0}, 3, 64);
    EXPECT_EQ(maxSets(smith, pool), 1);
    pool.clear();
    addToPool(pool, {"diamond_sword", 0}, 1, 1);
    addToPool(pool, {"netherite_ingot", 0}, 1, 64);
    EXPECT_EQ(assign(smith, pool, 1).missing, (std::vector<int>{0}));
}
