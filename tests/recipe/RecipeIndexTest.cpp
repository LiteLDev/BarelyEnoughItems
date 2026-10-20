#include "core/recipe/RecipeIndex.h"

#include <gtest/gtest.h>

using namespace bei::core;

namespace {
IngredientView one(char const* id, std::int16_t aux = 0, int count = 1) {
    return IngredientView{{ItemKey{id, aux}}, count};
}

RecipeIndex sampleIndex() {
    std::vector<CategoryInfo> cats{
        {category::FURNACE, "", "Smelting", LayoutKind::Smelting, {ItemKey{"minecraft:furnace", 0}}, 20},
        {category::CRAFTING, "", "Crafting", LayoutKind::CraftingGrid, {ItemKey{"minecraft:crafting_table", 0}}, 10},
    };
    std::vector<RecipeView> recipes;
    RecipeView planks;
    planks.id       = "planks";
    planks.category = category::CRAFTING;
    planks.width = planks.height = 1;
    planks.inputs                = {one("minecraft:oak_log")};
    planks.outputs               = {one("minecraft:oak_planks", 0, 4)};
    recipes.push_back(planks);

    RecipeView sticks;
    sticks.id       = "sticks";
    sticks.category = category::CRAFTING;
    sticks.width    = 1;
    sticks.height   = 2;
    sticks.inputs   = {IngredientView{{ItemKey{"minecraft:oak_planks", ItemKey::ANY_AUX}}}, one("minecraft:oak_planks")};
    sticks.outputs  = {one("minecraft:stick", 0, 4)};
    recipes.push_back(sticks);

    RecipeView charcoal;
    charcoal.id        = "charcoal";
    charcoal.category  = category::FURNACE;
    charcoal.inputs    = {one("minecraft:oak_log")};
    charcoal.outputs   = {one("minecraft:charcoal")};
    charcoal.catalysts = {one("minecraft:coal")};
    recipes.push_back(charcoal);

    recipes.push_back(planks); // duplicate, must be dropped

    RecipeIndex idx;
    idx.build(cats, recipes);
    return idx;
}
} // namespace

TEST(RecipeIndex, DropsDuplicates) { EXPECT_EQ(sampleIndex().size(), 3u); }

TEST(RecipeIndex, CategoriesSortedByOrder) {
    auto idx = sampleIndex();
    ASSERT_EQ(idx.categories().size(), 2u);
    EXPECT_EQ(idx.categories()[0].id, category::CRAFTING);
}

TEST(RecipeIndex, LookupRecipes) {
    auto idx    = sampleIndex();
    auto groups = idx.lookup(ItemKey{"minecraft:stick", 0}, LookupMode::Recipes);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].category, category::CRAFTING);
    ASSERT_EQ(groups[0].recipes.size(), 1u);
    EXPECT_EQ(idx.recipe(groups[0].recipes[0]).id, "sticks");
}

TEST(RecipeIndex, LookupUsesOrderedByCategory) {
    auto idx    = sampleIndex();
    auto groups = idx.lookup(ItemKey{"minecraft:oak_log", 0}, LookupMode::Uses);
    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].category, category::CRAFTING);
    EXPECT_EQ(groups[1].category, category::FURNACE);
}

TEST(RecipeIndex, CatalystsAreNotUses) {
    auto idx = sampleIndex();
    EXPECT_FALSE(idx.has(ItemKey{"minecraft:coal", 0}, LookupMode::Uses));
}

TEST(RecipeIndex, WorkstationListsWholeCategory) {
    auto idx    = sampleIndex();
    auto groups = idx.lookup(ItemKey{"minecraft:furnace", 0}, LookupMode::Uses);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].category, category::FURNACE);
    EXPECT_EQ(groups[0].recipes.size(), 1u);
}

TEST(RecipeIndex, WildcardIngredientMatchesVariant) {
    auto idx = sampleIndex();
    EXPECT_TRUE(idx.has(ItemKey{"minecraft:oak_planks", 5}, LookupMode::Uses));
}
