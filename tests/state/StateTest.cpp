#include "core/state/Bookmarks.h"
#include "core/state/Config.h"
#include "core/state/Layout.h"
#include "core/state/NavHistory.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace bei::core;

namespace {
std::filesystem::path tempFile(char const* name) {
    return std::filesystem::temp_directory_path() / "bei-tests" / name;
}
} // namespace

TEST(Layout, FitGridAndHitTest) {
    auto g = fitGrid({10, 20, 100, 50}, 18);
    EXPECT_EQ(g.cols, 5);
    EXPECT_EQ(g.rows, 2);
    EXPECT_EQ(g.capacity(), 10);
    auto c = g.cellRect(6);
    EXPECT_EQ(g.hitTest(c.x + 1, c.y + 1), 6);
    EXPECT_EQ(g.hitTest(0, 0), -1);
}

TEST(Layout, PaginationWraps) {
    Pagination p{25, 10, 0};
    EXPECT_EQ(p.pageCount(), 3);
    p.step(-1);
    EXPECT_EQ(p.page, 2);
    p.step(1);
    EXPECT_EQ(p.page, 0);
    Pagination empty{0, 10, 5};
    empty.clamp();
    EXPECT_EQ(empty.page, 0);
    EXPECT_EQ(empty.pageCount(), 1);
}

TEST(NavHistory, PushBack) {
    NavHistory h;
    h.push({.item = ItemKey{"minecraft:stick", 0}});
    h.push({.item = ItemKey{"minecraft:stick", 0}}); // same as current: ignored
    h.push({.item = ItemKey{"minecraft:stone", 0}, .mode = LookupMode::Uses});
    EXPECT_EQ(h.depth(), 2u);
    auto prev = h.back();
    ASSERT_TRUE(prev);
    EXPECT_EQ(prev->item.id, "minecraft:stick");
    EXPECT_FALSE(h.back());
    EXPECT_EQ(h.depth(), 0u);
}

TEST(Bookmarks, ToggleAndPersist) {
    Bookmarks b;
    EXPECT_TRUE(b.toggle(ItemKey{"minecraft:stone", 0}));
    EXPECT_TRUE(b.toggle(ItemKey{"minecraft:wool", 3}));
    EXPECT_EQ(b.items().front().id, "minecraft:wool"); // newest first
    auto file = tempFile("bookmarks.json");
    ASSERT_TRUE(b.save(file));
    Bookmarks loaded;
    ASSERT_TRUE(loaded.load(file));
    EXPECT_EQ(loaded.items(), b.items());
    EXPECT_FALSE(loaded.toggle(ItemKey{"minecraft:stone", 0}));
    EXPECT_EQ(loaded.items().size(), 1u);
}

TEST(Config, PersistAndDefaults) {
    Config c;
    c.cheatMode  = true;
    c.lastSearch = "oak";
    auto file    = tempFile("config.json");
    ASSERT_TRUE(c.save(file));
    Config loaded;
    ASSERT_TRUE(loaded.load(file));
    EXPECT_TRUE(loaded.cheatMode);
    EXPECT_EQ(loaded.lastSearch, "oak");
    Config missing;
    EXPECT_FALSE(missing.load(tempFile("does-not-exist.json")));
    EXPECT_TRUE(missing.overlayEnabled);
}
