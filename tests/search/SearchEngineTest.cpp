#include "core/search/SearchEngine.h"

#include <gtest/gtest.h>

using namespace bei::core;

namespace {
std::vector<ItemEntry> sampleItems() {
    return {
        {ItemKey{"minecraft:oak_planks", 0}, "Oak Planks", "Oak Planks", {"minecraft:planks"}, "", 0},
        {ItemKey{"minecraft:stick", 0}, "Stick", "Stick", {}, "", 1},
        {ItemKey{"minecraft:stone", 0}, "Stone", "Stone", {}, "", 2},
    };
}
} // namespace

TEST(SearchEngine, EmptyQueryReturnsAll) {
    auto         items = sampleItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("").size(), 3u);
    EXPECT_EQ(engine.filter("   ").size(), 3u);
}

TEST(SearchEngine, CaseInsensitiveSubstring) {
    auto         items = sampleItems();
    SearchEngine engine;
    engine.setItems(&items);
    auto r = engine.filter("st");
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0], 1u);
    EXPECT_EQ(r[1], 2u);
    EXPECT_EQ(engine.filter("OAK").size(), 1u);
}

TEST(SearchEngine, AllTermsMustMatch) {
    auto         items = sampleItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("oak planks").size(), 1u);
    EXPECT_EQ(engine.filter("oak stone").size(), 0u);
}

TEST(SearchEngine, NoItemsIsSafe) {
    SearchEngine engine;
    EXPECT_TRUE(engine.filter("x").empty());
}
