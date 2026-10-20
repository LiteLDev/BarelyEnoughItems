#include "core/recipe/RecipeIndex.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include <string>

using namespace bei::core;

namespace {
IngredientView one(char const* id, std::int16_t aux = 0, int count = 1) {
    return IngredientView{{ItemKey{id, aux}}, count};
}

RecipeIndex sampleIndex() {
    std::vector<CategoryInfo> cats{
        {category::FURNACE,  "", "Smelting", LayoutKind::Smelting,     {ItemKey{"minecraft:furnace", 0}},        20},
        {category::CRAFTING, "", "Crafting", LayoutKind::CraftingGrid, {ItemKey{"minecraft:crafting_table", 0}}, 10},
    };
    std::vector<RecipeView> recipes;
    RecipeView              planks;
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
    sticks.inputs  = {IngredientView{{ItemKey{"minecraft:oak_planks", ItemKey::ANY_AUX}}}, one("minecraft:oak_planks")};
    sticks.outputs = {one("minecraft:stick", 0, 4)};
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

// ---------------------------------------------------------------------------------------------
// Extended coverage
// ---------------------------------------------------------------------------------------------

namespace {

constexpr std::int16_t ANY = ItemKey::ANY_AUX;

IngredientView alts(std::vector<ItemKey> keys) { return IngredientView{std::move(keys), 1}; }

RecipeView make(
    std::string                 id,
    std::string                 cat,
    std::vector<IngredientView> in,
    std::vector<IngredientView> out,
    std::vector<IngredientView> catalysts = {}
) {
    RecipeView r;
    r.id        = std::move(id);
    r.category  = std::move(cat);
    r.inputs    = std::move(in);
    r.outputs   = std::move(out);
    r.catalysts = std::move(catalysts);
    return r;
}

CategoryInfo cat(std::string id, int order, std::vector<ItemKey> stations = {}) {
    CategoryInfo c;
    c.id           = std::move(id);
    c.order        = order;
    c.workstations = std::move(stations);
    return c;
}

std::vector<std::string> ids(RecipeIndex const& idx, CategoryGroup const& g) {
    std::vector<std::string> out;
    for (auto i : g.recipes) out.push_back(idx.recipe(i).id);
    return out;
}

std::vector<std::string> cats(std::vector<CategoryGroup> const& groups) {
    std::vector<std::string> out;
    for (auto const& g : groups) out.push_back(g.category);
    return out;
}

// Straightforward reference implementation of the documented semantics, used to cross-check the index.
std::vector<CategoryGroup> reference(RecipeIndex const& idx, ItemKey const& item, LookupMode mode) {
    auto anyMatches = [&](std::vector<IngredientView> const& slots) {
        for (auto const& s : slots)
            for (auto const& a : s.alternatives)
                if (!a.empty() && a.matches(item)) return true;
        return false;
    };
    std::vector<std::string> order;
    for (auto const& c : idx.categories()) order.push_back(c.id);
    std::set<std::string> unknown;
    for (std::size_t i = 0; i < idx.size(); ++i)
        if (!idx.findCategory(idx.recipe(i).category)) unknown.insert(idx.recipe(i).category);
    order.insert(order.end(), unknown.begin(), unknown.end());

    std::vector<CategoryGroup> stationGroups, direct;
    std::set<std::string>      stationCats;
    if (mode == LookupMode::Uses) {
        for (auto const& c : idx.categories()) {
            bool hit = std::any_of(c.workstations.begin(), c.workstations.end(), [&](auto const& w) {
                return w.matches(item);
            });
            if (!hit) continue;
            std::vector<std::size_t> all;
            for (std::size_t i = 0; i < idx.size(); ++i)
                if (idx.recipe(i).category == c.id) all.push_back(i);
            if (all.empty()) continue;
            stationGroups.push_back({c.id, all});
            stationCats.insert(c.id);
        }
    }
    for (auto const& c : order) {
        if (stationCats.contains(c)) continue;
        CategoryGroup g{c, {}};
        for (std::size_t i = 0; i < idx.size(); ++i) {
            auto const& r = idx.recipe(i);
            if (r.category != c) continue;
            if (mode == LookupMode::Recipes ? anyMatches(r.outputs) : anyMatches(r.inputs)) g.recipes.push_back(i);
        }
        if (!g.recipes.empty()) direct.push_back(std::move(g));
    }
    stationGroups.insert(stationGroups.end(), direct.begin(), direct.end());
    return stationGroups;
}

bool sameGroups(std::vector<CategoryGroup> const& a, std::vector<CategoryGroup> const& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].category != b[i].category || a[i].recipes != b[i].recipes) return false;
    return true;
}

} // namespace

TEST(RecipeIndex, EmptyIndex) {
    RecipeIndex idx;
    EXPECT_EQ(idx.size(), 0u);
    EXPECT_TRUE(idx.categories().empty());
    EXPECT_TRUE(idx.lookup({"minecraft:stone", 0}, LookupMode::Recipes).empty());
    EXPECT_TRUE(idx.lookup({"minecraft:stone", ANY}, LookupMode::Uses).empty());
    EXPECT_FALSE(idx.has({"minecraft:stone", 0}, LookupMode::Uses));
    EXPECT_EQ(idx.lookupCount({"minecraft:stone", 0}, LookupMode::Uses), 0u);
    EXPECT_TRUE(idx.recipesInCategory(category::CRAFTING).empty());
    EXPECT_EQ(idx.findCategory(category::CRAFTING), nullptr);

    idx.build({}, {});
    EXPECT_EQ(idx.size(), 0u);
    EXPECT_FALSE(idx.has({"minecraft:stone", 0}, LookupMode::Recipes));
}

TEST(RecipeIndex, EmptyItemMatchesNothing) {
    RecipeIndex idx;
    idx.build(
        {
            cat("c", 1)
    },
        {make("r", "c", {alts({ItemKey{"", 0}})}, {one("minecraft:a")})}
    );
    EXPECT_TRUE(idx.lookup({}, LookupMode::Uses).empty());
    EXPECT_FALSE(idx.has({}, LookupMode::Uses));
    EXPECT_EQ(idx.lookupCount({}, LookupMode::Uses), 0u);
}

TEST(RecipeIndex, ClearResets) {
    auto idx = sampleIndex();
    idx.clear();
    EXPECT_EQ(idx.size(), 0u);
    EXPECT_FALSE(idx.has({"minecraft:stick", 0}, LookupMode::Recipes));
    EXPECT_EQ(idx.findCategory(category::CRAFTING), nullptr);
    EXPECT_TRUE(idx.recipesInCategory(category::CRAFTING).empty());
}

TEST(RecipeIndex, RebuildReplacesPreviousContent) {
    auto idx = sampleIndex();
    idx.build({cat("c", 1)}, {make("x", "c", {one("minecraft:a")}, {one("minecraft:b")})});
    EXPECT_EQ(idx.size(), 1u);
    EXPECT_FALSE(idx.has({"minecraft:stick", 0}, LookupMode::Recipes));
    EXPECT_TRUE(idx.has({"minecraft:b", 0}, LookupMode::Recipes));
}

TEST(RecipeIndex, CopiedIndexIsIndependent) {
    auto copy = sampleIndex();
    auto idx  = copy;
    copy.clear();
    EXPECT_TRUE(idx.has({"minecraft:stick", 0}, LookupMode::Recipes));
}

TEST(RecipeIndex, WildcardQueryMatchesAllVariants) {
    RecipeIndex idx;
    idx.build(
        {cat("c", 1)},
        {
            make("w0", "c", {one("minecraft:wool", 0)}, {one("minecraft:bed", 0)}),
            make("w1", "c", {one("minecraft:wool", 1)}, {one("minecraft:bed", 1)}),
            make("w2", "c", {one("minecraft:wool", 2)}, {one("minecraft:carpet", 2)}),
        }
    );
    auto r = idx.lookup({"minecraft:bed", ANY}, LookupMode::Recipes);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(ids(idx, r[0]), (std::vector<std::string>{"w0", "w1"}));

    auto u = idx.lookup({"minecraft:wool", ANY}, LookupMode::Uses);
    ASSERT_EQ(u.size(), 1u);
    EXPECT_EQ(ids(idx, u[0]), (std::vector<std::string>{"w0", "w1", "w2"}));

    auto exact = idx.lookup({"minecraft:wool", 1}, LookupMode::Uses);
    ASSERT_EQ(exact.size(), 1u);
    EXPECT_EQ(ids(idx, exact[0]), (std::vector<std::string>{"w1"}));
    EXPECT_FALSE(idx.has({"minecraft:wool", 7}, LookupMode::Uses));
    EXPECT_TRUE(idx.has({"minecraft:wool", ANY}, LookupMode::Uses));
    EXPECT_EQ(idx.lookupCount({"minecraft:wool", ANY}, LookupMode::Uses), 3u);
}

TEST(RecipeIndex, WildcardIngredientAndExactMerged) {
    RecipeIndex idx;
    idx.build(
        {cat("c", 1)},
        {
            make("exact", "c", {one("minecraft:planks", 3)}, {one("minecraft:a")}),
            make("wild", "c", {one("minecraft:planks", ANY)}, {one("minecraft:b")}),
            make("other", "c", {one("minecraft:planks", 4)}, {one("minecraft:c")}),
        }
    );
    auto u = idx.lookup({"minecraft:planks", 3}, LookupMode::Uses);
    ASSERT_EQ(u.size(), 1u);
    EXPECT_EQ(ids(idx, u[0]), (std::vector<std::string>{"exact", "wild"}));
    auto v = idx.lookup({"minecraft:planks", 9}, LookupMode::Uses);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(ids(idx, v[0]), (std::vector<std::string>{"wild"}));
    EXPECT_TRUE(idx.has({"minecraft:planks", 9}, LookupMode::Uses));
    EXPECT_FALSE(idx.has({"minecraft:planks", 9}, LookupMode::Recipes));
}

TEST(RecipeIndex, WildcardOutputMatchesConcreteQuery) {
    RecipeIndex idx;
    idx.build({cat("c", 1)}, {make("r", "c", {}, {one("minecraft:dye", ANY)})});
    EXPECT_TRUE(idx.has({"minecraft:dye", 4}, LookupMode::Recipes));
    EXPECT_EQ(idx.lookupCount({"minecraft:dye", 4}, LookupMode::Recipes), 1u);
    EXPECT_FALSE(idx.has({"minecraft:dyes", 4}, LookupMode::Recipes));
}

TEST(RecipeIndex, MultiAlternativeSlotsMatchEachRecipeOnce) {
    RecipeIndex idx;
    auto        slot = alts({
        ItemKey{"minecraft:oak_planks",   0  },
        ItemKey{"minecraft:birch_planks", 0  },
        ItemKey{"minecraft:oak_planks",   ANY}
    });
    idx.build({cat("c", 1)}, {make("chest", "c", {slot, slot, slot, slot}, {one("minecraft:chest")})});
    for (auto const& k : {
             ItemKey{"minecraft:oak_planks",   0  },
             ItemKey{"minecraft:birch_planks", 0  },
             ItemKey{"minecraft:oak_planks",   2  },
             ItemKey{"minecraft:oak_planks",   ANY},
             ItemKey{"minecraft:birch_planks", ANY}
    }) {
        auto g = idx.lookup(k, LookupMode::Uses);
        ASSERT_EQ(g.size(), 1u) << k.toString();
        EXPECT_EQ(g[0].recipes.size(), 1u) << k.toString();
    }
    EXPECT_FALSE(idx.has({"minecraft:birch_planks", 1}, LookupMode::Uses));
}

TEST(RecipeIndex, SameIdInDifferentCategoriesKept) {
    RecipeIndex idx;
    idx.build(
        {cat("a", 1), cat("b", 2)},
        {
            make("same", "a", {one("minecraft:x")}, {one("minecraft:y")}),
            make("same", "b", {one("minecraft:x")}, {one("minecraft:y")}),
            make("same", "a", {one("minecraft:z")}, {one("minecraft:y")}),
            make("", "a", {one("minecraft:x")}, {one("minecraft:y")}),
            make("", "a", {one("minecraft:x")}, {one("minecraft:y")}),
        }
    );
    EXPECT_EQ(idx.size(), 4u); // the second ("same", "a") is dropped; empty ids are never deduplicated
    EXPECT_FALSE(idx.has({"minecraft:z", 0}, LookupMode::Uses)); // first one wins
    auto g = idx.lookup({"minecraft:y", 0}, LookupMode::Recipes);
    EXPECT_EQ(cats(g), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(g[0].recipes.size(), 3u);
    EXPECT_EQ(g[1].recipes.size(), 1u);
}

TEST(RecipeIndex, WorkstationCategoriesFirstWithoutDuplicates) {
    RecipeIndex idx;
    // "tool" has a later order than "early", but its workstation is the queried item.
    idx.build(
        {
            cat("early", 1),
            cat("tool", 50, {ItemKey{"minecraft:table", 0}}
              )
    },
        {
            make("t1", "tool", {one("minecraft:iron")}, {one("minecraft:pick")}),
            make("e1", "early", {one("minecraft:table")}, {one("minecraft:upgraded_table")}),
            make("t2", "tool", {one("minecraft:table")}, {one("minecraft:big_table")}),
            make("t3", "tool", {one("minecraft:gold")}, {one("minecraft:sword")}),
        }
    );
    auto g = idx.lookup({"minecraft:table", 0}, LookupMode::Uses);
    EXPECT_EQ(cats(g), (std::vector<std::string>{"tool", "early"}));
    EXPECT_EQ(ids(idx, g[0]), (std::vector<std::string>{"t1", "t2", "t3"}));
    EXPECT_EQ(ids(idx, g[1]), (std::vector<std::string>{"e1"}));
    EXPECT_EQ(idx.lookupCount({"minecraft:table", 0}, LookupMode::Uses), 4u);
    // Being a workstation does not make an item the output of anything.
    EXPECT_FALSE(idx.has({"minecraft:table", 0}, LookupMode::Recipes));
}

TEST(RecipeIndex, WorkstationWildcardsBothDirections) {
    RecipeIndex idx;
    idx.build(
        {
            cat("anvil", 1, {ItemKey{"minecraft:anvil", ANY}}
              ),
            cat("loom", 2, {ItemKey{"minecraft:loom", 2}   }
              )
    },
        {make("a", "anvil", {}, {one("minecraft:x")}), make("l", "loom", {}, {one("minecraft:y")})}
    );
    EXPECT_TRUE(idx.has({"minecraft:anvil", 1}, LookupMode::Uses));
    EXPECT_TRUE(idx.has({"minecraft:loom", ANY}, LookupMode::Uses));
    EXPECT_TRUE(idx.has({"minecraft:loom", 2}, LookupMode::Uses));
    EXPECT_FALSE(idx.has({"minecraft:loom", 1}, LookupMode::Uses));
    EXPECT_EQ(cats(idx.lookup({"minecraft:anvil", 2}, LookupMode::Uses)), (std::vector<std::string>{"anvil"}));
}

TEST(RecipeIndex, WorkstationOfMultipleCategoriesAndEmptyCategory) {
    RecipeIndex idx;
    ItemKey     furnace{"minecraft:furnace", 0};
    idx.build(
        {cat("fuel", 30, {furnace}), cat("smelt", 20, {furnace}), cat("nothing", 10, {furnace})},
        {make("f", "fuel", {one("minecraft:coal")}, {}), make("s", "smelt", {one("minecraft:ore")}, {})}
    );
    EXPECT_EQ(cats(idx.lookup(furnace, LookupMode::Uses)), (std::vector<std::string>{"smelt", "fuel"}));
    RecipeIndex empty;
    empty.build({cat("nothing", 10, {furnace})}, {});
    EXPECT_FALSE(empty.has(furnace, LookupMode::Uses));
    EXPECT_TRUE(empty.lookup(furnace, LookupMode::Uses).empty());
}

TEST(RecipeIndex, CategoryOrderingAndUnknownCategories) {
    RecipeIndex idx;
    idx.build(
        {cat("z", 1), cat("b", 5), cat("a", 5), cat("a", 0)},
        {
            make("1", "zz_unknown", {one("minecraft:x")}, {}),
            make("2", "b", {one("minecraft:x")}, {}),
            make("3", "a", {one("minecraft:x")}, {}),
            make("4", "aa_unknown", {one("minecraft:x")}, {}),
            make("5", "z", {one("minecraft:x")}, {}),
            make("6", "b", {one("minecraft:x")}, {}),
        }
    );
    // Duplicate category id "a": the first after sorting (order 0) wins.
    ASSERT_EQ(idx.categories().size(), 3u);
    EXPECT_EQ(idx.categories()[0].id, "a");
    EXPECT_EQ(idx.categories()[0].order, 0);
    EXPECT_EQ(idx.findCategory("a")->order, 0);
    auto g = idx.lookup({"minecraft:x", 0}, LookupMode::Uses);
    EXPECT_EQ(cats(g), (std::vector<std::string>{"a", "z", "b", "aa_unknown", "zz_unknown"}));
    EXPECT_EQ(ids(idx, g[2]), (std::vector<std::string>{"2", "6"}));
    EXPECT_EQ(idx.recipesInCategory("b"), (std::vector<std::size_t>{1, 5}));
    EXPECT_EQ(idx.recipesInCategory("zz_unknown"), (std::vector<std::size_t>{0}));
    EXPECT_EQ(idx.categorySize("b"), 2u);
    EXPECT_EQ(idx.categorySize("missing"), 0u);
    // recipe() indices keep insertion order.
    EXPECT_EQ(idx.recipe(0).id, "1");
    EXPECT_EQ(idx.recipe(5).id, "6");
}

TEST(RecipeIndex, DuplicateCategoriesMergeWorkstations) {
    RecipeIndex idx;
    ItemKey     table{"minecraft:crafting_table", 0};
    ItemKey     bench{"mod:workbench", 0};
    idx.build(
        {cat("craft", 10, {table}), cat("craft", 20, {bench, table})},
        {make("r", "craft", {}, {one("minecraft:x")})}
    );
    ASSERT_EQ(idx.categories().size(), 1u);
    EXPECT_EQ(idx.categories()[0].order, 10);
    EXPECT_EQ(idx.categories()[0].workstations, (std::vector<ItemKey>{table, bench}));
    EXPECT_TRUE(idx.has(bench, LookupMode::Uses));
    EXPECT_EQ(cats(idx.lookup(bench, LookupMode::Uses)), (std::vector<std::string>{"craft"}));
    EXPECT_EQ(cats(idx.lookup(table, LookupMode::Uses)), (std::vector<std::string>{"craft"}));
}

TEST(RecipeIndex, CatalystOnlyItemHasNoUses) {
    RecipeIndex idx;
    idx.build(
        {
            cat("smelt", 1)
    },
        {make(
            "s",
            "smelt",
            {one("minecraft:ore")},
            {one("minecraft:ingot")},
            {one("minecraft:coal"), alts({ItemKey{"minecraft:log", ANY}})}
        )}
    );
    EXPECT_FALSE(idx.has({"minecraft:coal", 0}, LookupMode::Uses));
    EXPECT_FALSE(idx.has({"minecraft:log", 3}, LookupMode::Uses));
    EXPECT_FALSE(idx.has({"minecraft:coal", 0}, LookupMode::Recipes));
    EXPECT_TRUE(idx.has({"minecraft:ore", 0}, LookupMode::Uses));
}

TEST(RecipeIndex, MatchesReferenceOnRandomData) {
    std::mt19937             rng(1234);
    std::vector<std::string> items;
    for (int i = 0; i < 12; ++i) items.push_back("t:item" + std::to_string(i));
    auto randKey = [&] {
        std::int16_t aux = rng() % 4 == 0 ? ANY : static_cast<std::int16_t>(rng() % 3);
        return ItemKey{items[rng() % items.size()], aux};
    };
    auto randSlots = [&](unsigned maxSlots) {
        std::vector<IngredientView> slots(rng() % (maxSlots + 1));
        for (auto& s : slots) {
            unsigned n = rng() % 3;
            for (unsigned i = 0; i < n; ++i) s.alternatives.push_back(randKey());
        }
        return slots;
    };
    for (int round = 0; round < 20; ++round) {
        std::vector<CategoryInfo> categories;
        for (int c = 0; c < 4; ++c) {
            std::vector<ItemKey> stations;
            if (rng() % 2) stations.push_back(randKey());
            categories.push_back(cat("c" + std::to_string(c), static_cast<int>(rng() % 3), stations));
        }
        std::vector<RecipeView> recipes;
        for (int r = 0; r < 60; ++r) {
            std::string category = "c" + std::to_string(rng() % 6); // c4/c5 are unregistered
            std::string id       = "r" + std::to_string(rng() % 40);
            recipes.push_back(make(id, category, randSlots(4), randSlots(2), randSlots(1)));
        }
        RecipeIndex idx;
        idx.build(categories, recipes);
        for (auto const& name : items) {
            for (std::int16_t aux : {std::int16_t{0}, std::int16_t{1}, std::int16_t{2}, std::int16_t{5}, ANY}) {
                ItemKey k{name, aux};
                for (auto mode : {LookupMode::Recipes, LookupMode::Uses}) {
                    auto got = idx.lookup(k, mode);
                    auto exp = reference(idx, k, mode);
                    ASSERT_TRUE(sameGroups(got, exp)) << "round " << round << " key " << k.toString();
                    EXPECT_EQ(idx.has(k, mode), !exp.empty());
                    std::size_t n = 0;
                    for (auto const& g : exp) n += g.recipes.size();
                    EXPECT_EQ(idx.lookupCount(k, mode), n);
                }
            }
        }
        for (auto const& c : idx.categories()) EXPECT_EQ(idx.categorySize(c.id), idx.recipesInCategory(c.id).size());
    }
}

TEST(RecipeIndex, PerformanceSanityLargeIndex) {
    constexpr int            kItems   = 3000;
    constexpr int            kRecipes = 5000;
    std::mt19937             rng(42);
    std::vector<std::string> items;
    items.reserve(kItems);
    for (int i = 0; i < kItems; ++i) items.push_back("minecraft:item_" + std::to_string(i));

    std::vector<CategoryInfo> categories;
    for (int c = 0; c < 12; ++c)
        categories.push_back(
            cat("cat" + std::to_string(c),
                c,
                {
                    ItemKey{items[c], 0}
        })
        );
    std::vector<RecipeView> recipes;
    recipes.reserve(kRecipes);
    for (int r = 0; r < kRecipes; ++r) {
        std::vector<IngredientView> in(9);
        for (auto& slot : in) {
            for (int a = 0; a < 30; ++a) {
                auto aux = rng() % 8 == 0 ? ANY : static_cast<std::int16_t>(rng() % 16);
                slot.alternatives.push_back(ItemKey{items[rng() % kItems], aux});
            }
        }
        auto out = one(items[r % kItems].c_str());
        recipes.push_back(make("r" + std::to_string(r), "cat" + std::to_string(r % 12), std::move(in), {out}));
    }

    RecipeIndex idx;
    auto        t0 = std::chrono::steady_clock::now();
    idx.build(categories, recipes);
    auto t1 = std::chrono::steady_clock::now();

    std::size_t total = 0, hits = 0;
    for (int i = 0; i < kItems; ++i) {
        for (std::int16_t aux : {std::int16_t{0}, std::int16_t{3}, ANY}) {
            ItemKey k{items[i], aux};
            hits += idx.has(k, LookupMode::Uses) ? 1 : 0;
            hits += idx.has(k, LookupMode::Recipes) ? 1 : 0;
            for (auto const& g : idx.lookup(k, LookupMode::Uses)) total += g.recipes.size();
            for (auto const& g : idx.lookup(k, LookupMode::Recipes)) total += g.recipes.size();
        }
    }
    auto t2 = std::chrono::steady_clock::now();

    auto buildMs  = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto lookupMs = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::printf(
        "[perf] build %lld ms; 18000 lookups + 18000 has() %lld ms; %zu results, %zu hits\n",
        static_cast<long long>(buildMs),
        static_cast<long long>(lookupMs),
        total,
        hits
    );
    EXPECT_GT(total, 0u);
    // Generous bounds that only catch gross regressions (a linear scan takes many seconds here).
    EXPECT_LT(buildMs, 5000);
    EXPECT_LT(lookupMs, 3000);
}
