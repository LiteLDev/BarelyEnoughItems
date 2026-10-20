#include "core/state/AtomicWrite.h"
#include "core/state/Bookmarks.h"
#include "core/state/Config.h"
#include "core/state/Layout.h"
#include "core/state/NavHistory.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

using namespace bei::core;

namespace {
std::filesystem::path tempFile(char const* name) { return std::filesystem::temp_directory_path() / "bei-tests" / name; }

void writeRaw(std::filesystem::path const& file, std::string const& text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary | std::ios::trunc) << text;
}

std::string readRaw(std::filesystem::path const& file) { return readFile(file).value_or(""); }

bool nonNegative(Rect const& r) { return r.w >= 0 && r.h >= 0 && std::isfinite(r.x) && std::isfinite(r.y); }

bool inside(Rect const& inner, Rect const& outer) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.right() <= outer.right() + 1e-3f
        && inner.bottom() <= outer.bottom() + 1e-3f;
}

void expectSane(OverlayLayout const& l, float W, float H) {
    Rect screen{0, 0, W, H};
    for (Rect const* r :
         {&l.itemPanel,
          &l.bookmarkPanel,
          &l.itemPageControls,
          &l.itemGrid,
          &l.searchBar,
          &l.bookmarkPageControls,
          &l.bookmarkGrid}) {
        EXPECT_TRUE(nonNegative(*r)) << r->x << "," << r->y << " " << r->w << "x" << r->h;
        if (!r->empty()) EXPECT_TRUE(inside(*r, screen)) << r->x << "," << r->y << " " << r->w << "x" << r->h;
    }
}

const Rect INVENTORY{(427 - 176) / 2.f, (240 - 166) / 2.f, 176, 166}; // classic inventory on a 427x240 GUI
} // namespace

// ---------------------------------------------------------------- Rect / GridLayout

TEST(Rect, Helpers) {
    Rect r{10, 20, 30, 40};
    EXPECT_FLOAT_EQ(r.centerX(), 25);
    EXPECT_FLOAT_EQ(r.centerY(), 40);
    EXPECT_EQ(r.inset(5), (Rect{15, 25, 20, 30}));
    EXPECT_TRUE(r.inset(100).empty());
    EXPECT_EQ(r.intersect({0, 0, 20, 30}), (Rect{10, 20, 10, 10}));
    EXPECT_TRUE(r.intersect({100, 100, 5, 5}).empty());
    EXPECT_TRUE((Rect{0, 0, std::nanf(""), 5}).empty());
}

TEST(Layout, FitGridAndHitTest) {
    auto g = fitGrid({10, 20, 100, 50}, 18);
    EXPECT_EQ(g.cols, 5);
    EXPECT_EQ(g.rows, 2);
    EXPECT_EQ(g.capacity(), 10);
    auto c = g.cellRect(6);
    EXPECT_EQ(g.hitTest(c.x + 1, c.y + 1), 6);
    EXPECT_EQ(g.hitTest(0, 0), -1);
}

TEST(Layout, FitGridCentersHorizontallyTopAligned) {
    auto g = fitGrid({0, 0, 100, 100}, 18);
    EXPECT_EQ(g.cols, 5);
    EXPECT_FLOAT_EQ(g.bounds.x, 5); // (100 - 90) / 2
    EXPECT_FLOAT_EQ(g.bounds.y, 0);
    auto v = fitGrid({0, 0, 100, 100}, 18, 0, true);
    EXPECT_FLOAT_EQ(v.bounds.y, 5);
}

TEST(Layout, FitGridMaxCols) {
    auto g = fitGrid({0, 0, 180, 36}, 18, 4);
    EXPECT_EQ(g.cols, 4);
    EXPECT_FLOAT_EQ(g.bounds.w, 72);
    EXPECT_FLOAT_EQ(g.bounds.x, 54);                      // centered
    EXPECT_EQ(fitGrid({0, 0, 180, 36}, 18, -3).cols, 10); // negative = unlimited
    EXPECT_EQ(fitGrid({0, 0, 180, 36}, 18, 100).cols, 10);
}

TEST(Layout, FitGridExactAndFloatError) {
    EXPECT_EQ(fitGrid({0, 0, 90, 18}, 18).cols, 5);
    EXPECT_EQ(fitGrid({0, 0, 89.99999f, 18}, 18).cols, 5);
    EXPECT_EQ(fitGrid({0, 0, 89, 18}, 18).cols, 4);
}

TEST(Layout, FitGridDegenerate) {
    for (auto g :
         {fitGrid({0, 0, 0, 100}, 18),
          fitGrid({0, 0, -50, 100}, 18),
          fitGrid({0, 0, 100, 100}, 0),
          fitGrid({0, 0, 100, 100}, -18),
          fitGrid({0, 0, 100, 10}, 18),
          fitGrid({0, 0, 10, 100}, 18),
          fitGrid({0, 0, 100, 100}, std::numeric_limits<float>::infinity())}) {
        EXPECT_EQ(g.capacity(), 0);
        EXPECT_EQ(g.cols * g.rows, 0);
        EXPECT_EQ(g.hitTest(1, 1), -1);
        EXPECT_GE(g.bounds.w, 0);
        EXPECT_GE(g.bounds.h, 0);
    }
}

TEST(Layout, HitTestEdges) {
    auto g = fitGrid({0, 0, 36, 36}, 18); // 2x2 at origin
    EXPECT_EQ(g.hitTest(0, 0), 0);
    EXPECT_EQ(g.hitTest(17.99f, 0), 0);
    EXPECT_EQ(g.hitTest(18, 0), 1);
    EXPECT_EQ(g.hitTest(35.99f, 35.99f), 3);
    EXPECT_EQ(g.hitTest(36, 0), -1); // right edge exclusive
    EXPECT_EQ(g.hitTest(0, 36), -1); // bottom edge exclusive
    EXPECT_EQ(g.hitTest(-0.01f, 5), -1);
    EXPECT_EQ(g.hitTest(std::nanf(""), 5), -1);
    EXPECT_EQ(g.hitTest(20, 20, 3), -1); // index 3 past count
    EXPECT_EQ(g.hitTest(20, 1, 3), 1);
    EXPECT_EQ(g.hitTest(1, 1, 0), -1);
    for (int i = 0; i < g.capacity(); ++i) {
        auto c = g.cellRect(i);
        EXPECT_EQ(g.hitTest(c.x, c.y), i);
        EXPECT_EQ(g.hitTest(c.right() - 0.01f, c.bottom() - 0.01f), i);
    }
}

// ---------------------------------------------------------------- Pagination

TEST(Layout, PaginationWraps) {
    Pagination p{25, 10, 0};
    EXPECT_EQ(p.pageCount(), 3);
    p.step(-1);
    EXPECT_EQ(p.page, 2);
    p.step(1);
    EXPECT_EQ(p.page, 0);
    p.step(7);
    EXPECT_EQ(p.page, 1);
    p.step(-7);
    EXPECT_EQ(p.page, 0);
    Pagination empty{0, 10, 5};
    empty.clamp();
    EXPECT_EQ(empty.page, 0);
    EXPECT_EQ(empty.pageCount(), 1);
}

TEST(Pagination, ZeroItemsAndBadPerPage) {
    Pagination p{0, 10, 0};
    EXPECT_EQ(p.pageCount(), 1);
    EXPECT_EQ(p.firstIndex(), 0);
    EXPECT_EQ(p.visibleCount(), 0);
    EXPECT_TRUE(p.isFirstPage());
    EXPECT_TRUE(p.isLastPage());
    p.step(1);
    EXPECT_EQ(p.page, 0);
    p.step(-1);
    EXPECT_EQ(p.page, 0);

    Pagination bad{50, 0, 3};
    EXPECT_EQ(bad.pageCount(), 1);
    EXPECT_EQ(bad.firstIndex(), 0);
    bad.clamp();
    EXPECT_EQ(bad.page, 0);
    Pagination neg{50, -4, 0};
    EXPECT_EQ(neg.pageCount(), 1);
    EXPECT_EQ(neg.firstIndex(), 0);
    EXPECT_EQ(neg.visibleCount(), 0);
}

TEST(Pagination, StepFromOutOfRangePage) {
    Pagination p{25, 10, 9}; // stale page after the list shrank
    p.step(1);
    EXPECT_EQ(p.page, 0); // clamped to 2, then wrapped
    Pagination q{25, 10, -5};
    q.step(-1);
    EXPECT_EQ(q.page, 2);
}

TEST(Pagination, VisibleCountAndPageOf) {
    Pagination p{25, 10, 0};
    EXPECT_EQ(p.visibleCount(), 10);
    p.page = 2;
    EXPECT_EQ(p.visibleCount(), 5);
    EXPECT_TRUE(p.isLastPage());
    EXPECT_EQ(p.pageOf(0), 0);
    EXPECT_EQ(p.pageOf(9), 0);
    EXPECT_EQ(p.pageOf(10), 1);
    EXPECT_EQ(p.pageOf(24), 2);
    EXPECT_EQ(p.pageOf(1000), 2);
    EXPECT_EQ(p.pageOf(-3), 0);
}

TEST(Pagination, JumpTo) {
    Pagination p{25, 10, 0};
    p.jumpTo(1);
    EXPECT_EQ(p.page, 1);
    p.jumpTo(99);
    EXPECT_EQ(p.page, 2);
    p.jumpTo(-1);
    EXPECT_EQ(p.page, 0);
    p.showIndex(15);
    EXPECT_EQ(p.page, 1);
    EXPECT_EQ(p.firstIndex(), 10);
}

TEST(Pagination, UpdateKeepsFirstVisibleItem) {
    Pagination p{100, 10, 3}; // showing 30..39
    p.update(100, 7);         // panel resized: page containing item 30
    EXPECT_EQ(p.page, 4);     // 28..34
    EXPECT_LE(p.firstIndex(), 30);
    EXPECT_GT(p.firstIndex() + p.perPage, 30);

    p.update(100, 20); // larger page: 20..39
    EXPECT_EQ(p.page, 1);

    p.update(25, 20); // list shrank past the anchor: stays on the last page
    EXPECT_EQ(p.page, 1);
    p.update(5, 20);
    EXPECT_EQ(p.page, 0);

    p.update(0, 20);
    EXPECT_EQ(p.page, 0);
    EXPECT_EQ(p.itemCount, 0);

    p.update(-3, 0);
    EXPECT_EQ(p.itemCount, 0);
    EXPECT_EQ(p.page, 0);

    Pagination q{100, 10, 5};
    q.update(200, 10); // more items: page unchanged
    EXPECT_EQ(q.page, 5);
}

// ---------------------------------------------------------------- Overlay layout

TEST(OverlayLayout, PanelsBesideContainer) {
    auto l = computeOverlayLayout(427, 240, INVENTORY);
    expectSane(l, 427, 240);
    EXPECT_GE(l.itemPanel.x, INVENTORY.right() + 4);
    EXPECT_LE(l.itemPanel.right(), 427 - 4);
    EXPECT_LE(l.bookmarkPanel.right(), INVENTORY.x - 4);
    EXPECT_GE(l.bookmarkPanel.x, 4);
    EXPECT_FLOAT_EQ(l.itemPanel.y, 4);
    EXPECT_FLOAT_EQ(l.itemPanel.bottom(), 236);
    EXPECT_TRUE(l.itemPanelUsable);
    EXPECT_TRUE(l.bookmarkPanelUsable);
    EXPECT_FALSE(l.searchBarCentered);

    // Sub-areas: page controls on top, search bar at the bottom, grid in between.
    EXPECT_TRUE(inside(l.itemPageControls, l.itemPanel));
    EXPECT_TRUE(inside(l.itemGrid, l.itemPanel));
    EXPECT_TRUE(inside(l.searchBar, l.itemPanel));
    EXPECT_FLOAT_EQ(l.itemPageControls.y, l.itemPanel.y);
    EXPECT_FLOAT_EQ(l.searchBar.bottom(), l.itemPanel.bottom());
    EXPECT_FLOAT_EQ(l.searchBar.h, 18);
    EXPECT_FLOAT_EQ(l.itemPageControls.h, 12);
    EXPECT_GE(l.itemGrid.y, l.itemPageControls.bottom());
    EXPECT_LE(l.itemGrid.bottom(), l.searchBar.y);
    EXPECT_GT(fitGrid(l.itemGrid, 18).capacity(), 0);

    EXPECT_TRUE(inside(l.bookmarkGrid, l.bookmarkPanel));
    EXPECT_GE(l.bookmarkGrid.y, l.bookmarkPageControls.bottom());
    EXPECT_FLOAT_EQ(l.bookmarkGrid.bottom(), l.bookmarkPanel.bottom());
}

TEST(OverlayLayout, SnapsToWholeUnits) {
    Rect c{100.3f, 30.7f, 176.2f, 166};
    auto l = computeOverlayLayout(427.5f, 240.5f, c);
    expectSane(l, 427.5f, 240.5f);
    for (Rect const& r : {l.itemPanel, l.bookmarkPanel}) {
        EXPECT_FLOAT_EQ(r.x, std::round(r.x));
        EXPECT_FLOAT_EQ(r.y, std::round(r.y));
        EXPECT_FLOAT_EQ(r.w, std::round(r.w));
        EXPECT_FLOAT_EQ(r.h, std::round(r.h));
    }
    EXPECT_GE(l.itemPanel.x, c.right() + 4);
}

TEST(OverlayLayout, EmptyContainerUsesQuarters) {
    auto l = computeOverlayLayout(400, 200, Rect{});
    expectSane(l, 400, 200);
    EXPECT_FLOAT_EQ(l.itemPanel.x, 304);
    EXPECT_FLOAT_EQ(l.itemPanel.w, 92);
    EXPECT_FLOAT_EQ(l.bookmarkPanel.x, 4);
    EXPECT_FLOAT_EQ(l.bookmarkPanel.w, 92);
    EXPECT_TRUE(l.itemPanelUsable);
}

TEST(OverlayLayout, NoRoomBesideContainer) {
    // Container fills the whole screen width: no room for either panel.
    auto l = computeOverlayLayout(300, 200, Rect{0, 10, 300, 180});
    expectSane(l, 300, 200);
    EXPECT_FALSE(l.itemPanelUsable);
    EXPECT_FALSE(l.bookmarkPanelUsable);
    EXPECT_FLOAT_EQ(l.itemPanel.w, 0);
    EXPECT_FLOAT_EQ(l.bookmarkPanel.w, 0);
    EXPECT_EQ(fitGrid(l.itemGrid, 18).capacity(), 0);

    // A little room (less than the minimum usable width).
    auto narrow = computeOverlayLayout(300, 200, Rect{20, 10, 250, 180});
    expectSane(narrow, 300, 200);
    EXPECT_GT(narrow.itemPanel.w, 0);
    EXPECT_LT(narrow.itemPanel.w, OverlayLayoutOptions::DEFAULT_MIN_PANEL_WIDTH);
    EXPECT_FALSE(narrow.itemPanelUsable);
    EXPECT_FALSE(narrow.bookmarkPanelUsable);

    // Custom minimum.
    OverlayLayoutOptions o;
    o.minPanelWidth = 10;
    EXPECT_TRUE(computeOverlayLayout(300, 200, Rect{20, 10, 250, 180}, o).itemPanelUsable);
}

TEST(OverlayLayout, ContainerOffScreenOrOverhanging) {
    // Container larger than the screen gets clamped: no negative sizes.
    auto l = computeOverlayLayout(200, 100, Rect{-50, -50, 400, 300});
    expectSane(l, 200, 100);
    EXPECT_FALSE(l.itemPanelUsable);
    // Fully off-screen container behaves like an empty one.
    auto off = computeOverlayLayout(400, 200, Rect{1000, 1000, 50, 50});
    expectSane(off, 400, 200);
    EXPECT_FLOAT_EQ(off.itemPanel.x, 304);
}

TEST(OverlayLayout, DegenerateScreens) {
    float nan = std::nanf("");
    for (auto [w, h] : {
             std::pair{0.f,    0.f   },
             std::pair{-10.f,  50.f  },
             std::pair{5.f,    5.f   },
             std::pair{nan,    100.f },
             std::pair{1.f,    1000.f},
             std::pair{1000.f, 1.f   }
    }) {
        auto l = computeOverlayLayout(w, h, Rect{1, 1, 2, 2});
        expectSane(l, std::isfinite(w) && w > 0 ? w : 0, h > 0 ? h : 0);
        auto e = computeOverlayLayout(w, h, Rect{});
        expectSane(e, std::isfinite(w) && w > 0 ? w : 0, h > 0 ? h : 0);
    }
    // Short screen: grid gets zero height rather than negative.
    auto s = computeOverlayLayout(427, 30, Rect{125, 2, 176, 26});
    expectSane(s, 427, 30);
    EXPECT_FALSE(s.itemPanelUsable);
    // Huge margin is capped.
    auto m = computeOverlayLayout(427, 240, INVENTORY, 1000);
    expectSane(m, 427, 240);
}

TEST(OverlayLayout, UsableMeansGridHoldsACell) {
    // Sweep screen heights: whenever a panel is reported usable its grid must fit a cell, and
    // vice versa (given enough width).
    for (float h = 20; h < 90; h += 0.5f) {
        auto l = computeOverlayLayout(427, h, Rect{125, 0, 176, h});
        expectSane(l, 427, h);
        EXPECT_EQ(l.itemPanelUsable, fitGrid(l.itemGrid, 18).capacity() > 0) << h;
        EXPECT_EQ(l.bookmarkPanelUsable, fitGrid(l.bookmarkGrid, 18).capacity() > 0) << h;
    }
}

TEST(Layout, HugeGridDoesNotOverflow) {
    auto g = fitGrid({0, 0, 1e9f, 1e9f}, 1e-6f);
    EXPECT_GT(g.capacity(), 0);
    EXPECT_LE(g.cols, 1 << 15);
}

TEST(OverlayLayout, CenteredSearchBar) {
    OverlayLayoutOptions o;
    o.centerSearchBar = true;
    auto l            = computeOverlayLayout(427, 240, Rect{125, 20, 176, 166}, o);
    expectSane(l, 427, 240);
    ASSERT_TRUE(l.searchBarCentered);
    EXPECT_FLOAT_EQ(l.searchBar.h, 18);
    EXPECT_GE(l.searchBar.y, 186 + 4);
    EXPECT_NEAR(l.searchBar.centerX(), 125 + 88, 1);
    EXPECT_LE(l.searchBar.w, 176);
    // The grid now extends to the bottom of the item panel.
    EXPECT_FLOAT_EQ(l.itemGrid.bottom(), l.itemPanel.bottom());
    EXPECT_TRUE(l.itemPanelUsable);

    o.centerSearchBarMaxWidth = 100;
    auto narrow               = computeOverlayLayout(427, 240, Rect{125, 20, 176, 166}, o);
    EXPECT_LE(narrow.searchBar.w, 100);
    EXPECT_NEAR(narrow.searchBar.centerX(), 213, 1);

    // No room under the container: falls back into the item panel.
    auto fb = computeOverlayLayout(427, 240, Rect{125, 60, 176, 166}, o);
    EXPECT_FALSE(fb.searchBarCentered);
    EXPECT_TRUE(inside(fb.searchBar, fb.itemPanel));
    EXPECT_FLOAT_EQ(fb.searchBar.bottom(), fb.itemPanel.bottom());

    // No container at all: in-panel.
    auto none = computeOverlayLayout(427, 240, Rect{}, o);
    EXPECT_FALSE(none.searchBarCentered);
}

// ---------------------------------------------------------------- NavHistory

TEST(NavHistory, PushBack) {
    NavHistory h;
    h.push({
        .item = ItemKey{"minecraft:stick", 0}
    });
    h.push({
        .item = ItemKey{"minecraft:stick", 0}
    }); // same as current: ignored
    h.push({
        .item = ItemKey{"minecraft:stone", 0},
        .mode = LookupMode::Uses
    });
    EXPECT_EQ(h.depth(), 2u);
    EXPECT_TRUE(h.canGoBack());
    ASSERT_TRUE(h.previous());
    EXPECT_EQ(h.previous()->item.id, "minecraft:stick");
    auto prev = h.back();
    ASSERT_TRUE(prev);
    EXPECT_EQ(prev->item.id, "minecraft:stick");
    EXPECT_FALSE(h.canGoBack());
    EXPECT_FALSE(h.previous());
    EXPECT_FALSE(h.back());
    EXPECT_EQ(h.depth(), 0u);
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.back()); // back on empty is a no-op
    EXPECT_EQ(h.current(), nullptr);
}

TEST(NavHistory, MaxDepthTrimsOldest) {
    NavHistory h;
    for (int i = 0; i < 100; ++i)
        h.push({
            .item = ItemKey{"minecraft:item" + std::to_string(i), 0}
        });
    EXPECT_EQ(h.depth(), NavHistory::MAX_DEPTH);
    EXPECT_EQ(h.entries().front().item.id, "minecraft:item36");
    EXPECT_EQ(h.current()->item.id, "minecraft:item99");

    h.setMaxDepth(3);
    EXPECT_EQ(h.depth(), 3u);
    EXPECT_EQ(h.entries().front().item.id, "minecraft:item97");
    h.push({
        .item = ItemKey{"minecraft:new", 0}
    });
    EXPECT_EQ(h.depth(), 3u);
    EXPECT_EQ(h.entries().front().item.id, "minecraft:item98");

    h.setMaxDepth(0); // minimum 1
    EXPECT_EQ(h.maxDepth(), 1u);
    EXPECT_EQ(h.depth(), 1u);
    EXPECT_EQ(h.current()->item.id, "minecraft:new");
    EXPECT_FALSE(h.back());
}

TEST(NavHistory, UpdateCurrentTabAndPage) {
    NavHistory h;
    EXPECT_FALSE(h.setCurrentPage(2));
    EXPECT_FALSE(h.setCurrentTab(1));
    h.push({
        .item = ItemKey{"minecraft:stick", 0}
    });
    EXPECT_TRUE(h.setCurrentPage(3));
    EXPECT_EQ(h.current()->page, 3);
    EXPECT_TRUE(h.setCurrentTab(0)); // same tab: page kept
    EXPECT_EQ(h.current()->page, 3);
    EXPECT_TRUE(h.setCurrentTab(2, "minecraft:furnace"));
    EXPECT_EQ(h.current()->categoryTab, 2);
    EXPECT_EQ(h.current()->category, "minecraft:furnace");
    EXPECT_EQ(h.current()->page, 0);
    h.setCurrentPage(-4);
    EXPECT_EQ(h.current()->page, 0);
    h.setCurrentTab(-1);
    EXPECT_EQ(h.current()->categoryTab, 0);

    // State survives navigating away and back.
    h.setCurrentPage(5);
    h.push({
        .item = ItemKey{"minecraft:stone", 0}
    });
    auto prev = h.back();
    ASSERT_TRUE(prev);
    EXPECT_EQ(prev->page, 5);
    EXPECT_EQ(prev->category, "minecraft:furnace");

    NavHistory const& ch = h;
    ASSERT_NE(ch.current(), nullptr);
    EXPECT_EQ(ch.current()->page, 5);
}

TEST(NavHistory, PushOrReplace) {
    NavHistory h;
    EXPECT_TRUE(h.pushOrReplace({
        .item = ItemKey{"minecraft:stick", 0}
    }));
    h.setCurrentPage(4);
    // Same target, different page: push() would stack a duplicate, pushOrReplace replaces.
    EXPECT_FALSE(h.pushOrReplace({
        .item = ItemKey{"minecraft:stick", 0}
    }));
    EXPECT_EQ(h.depth(), 1u);
    EXPECT_EQ(h.current()->page, 0);
    EXPECT_TRUE(h.pushOrReplace({
        .item = ItemKey{"minecraft:stick", 0},
        .mode = LookupMode::Uses
    }));
    EXPECT_EQ(h.depth(), 2u);
    RecipeFocus a{
        .item = ItemKey{"minecraft:stick", 0},
        .page = 1
    };
    RecipeFocus b{
        .item = ItemKey{"minecraft:stick", 0},
        .page = 2
    };
    EXPECT_TRUE(a.sameTarget(b));
    EXPECT_FALSE(a == b);

    // Item focus: an empty category matches the category the user's tab filled in.
    h.clear();
    h.pushOrReplace({
        .item = ItemKey{"minecraft:stone", 0}
    });
    h.setCurrentTab(2, "minecraft:furnace");
    EXPECT_FALSE(h.pushOrReplace({
        .item = ItemKey{"minecraft:stone", 0}
    }));
    EXPECT_EQ(h.depth(), 1u);
    EXPECT_EQ(h.current()->category, "minecraft:furnace"); // user's tab kept
    EXPECT_EQ(h.current()->categoryTab, 2);
    EXPECT_TRUE(h.pushOrReplace({
        .item     = ItemKey{"minecraft:stone", 0},
        .category = "minecraft:crafting"
    }));
    // Category browsing: different categories are different targets, even vs. empty.
    RecipeFocus c1{.category = "minecraft:crafting"};
    RecipeFocus c0{};
    EXPECT_FALSE(c1.sameTarget(c0));
    EXPECT_TRUE(c1.sameTarget(RecipeFocus{.category = "minecraft:crafting", .page = 3}));
}

// ---------------------------------------------------------------- Bookmarks

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

TEST(Bookmarks, AddIgnoresEmptyAndDuplicates) {
    Bookmarks b;
    b.add(ItemKey{});
    EXPECT_TRUE(b.empty());
    b.add(ItemKey{"minecraft:stone", 0});
    b.add(ItemKey{"minecraft:stone", 0});
    EXPECT_EQ(b.size(), 1u);
    b.add(ItemKey{"minecraft:stone", 1}); // different aux is a different bookmark
    b.add(ItemKey{"minecraft:stone", ItemKey::ANY_AUX});
    EXPECT_EQ(b.size(), 3u);
    EXPECT_FALSE(b.toggle(ItemKey{}));
}

TEST(Bookmarks, InsertMoveReorder) {
    Bookmarks b;
    for (char const* id : {"minecraft:d", "minecraft:c", "minecraft:b", "minecraft:a"}) b.add(ItemKey{id, 0});
    auto ids = [&] {
        std::string s;
        for (auto const& k : b.items()) s += k.id.substr(10);
        return s;
    };
    EXPECT_EQ(ids(), "abcd");
    EXPECT_TRUE(b.insert(99, ItemKey{"minecraft:e", 0})); // clamped to the end
    EXPECT_EQ(ids(), "abcde");
    EXPECT_FALSE(b.insert(0, ItemKey{"minecraft:e", 0}));
    EXPECT_TRUE(b.insert(2, ItemKey{"minecraft:x", 0}));
    EXPECT_EQ(ids(), "abxcde");

    EXPECT_TRUE(b.moveTo(ItemKey{"minecraft:x", 0}, 0));
    EXPECT_EQ(ids(), "xabcde");
    EXPECT_TRUE(b.moveTo(ItemKey{"minecraft:x", 0}, 3));
    EXPECT_EQ(ids(), "abcxde");
    EXPECT_TRUE(b.moveTo(ItemKey{"minecraft:a", 0}, 1000)); // clamped to the last slot
    EXPECT_EQ(ids(), "bcxdea");
    EXPECT_TRUE(b.move(5, 0));
    EXPECT_EQ(ids(), "abcxde");
    EXPECT_TRUE(b.move(2, 2));
    EXPECT_EQ(ids(), "abcxde");
    EXPECT_FALSE(b.move(6, 0));
    EXPECT_FALSE(b.moveTo(ItemKey{"minecraft:zzz", 0}, 0));
    EXPECT_EQ(b.indexOf(ItemKey{"minecraft:x", 0}), 3u);
    EXPECT_FALSE(b.indexOf(ItemKey{"minecraft:zzz", 0}));

    Bookmarks empty;
    EXPECT_FALSE(empty.move(0, 0));
}

TEST(Bookmarks, MaxCount) {
    Bookmarks b;
    EXPECT_EQ(b.maxCount(), Bookmarks::MAX_COUNT);
    b.setMaxCount(3);
    for (int i = 0; i < 5; ++i) b.add(ItemKey{"minecraft:i" + std::to_string(i), 0});
    EXPECT_EQ(b.size(), 3u);
    EXPECT_TRUE(b.full());
    EXPECT_EQ(b.items().front().id, "minecraft:i2"); // later adds were refused
    EXPECT_FALSE(b.toggle(ItemKey{"minecraft:new", 0}));
    EXPECT_FALSE(b.contains(ItemKey{"minecraft:new", 0}));
    EXPECT_FALSE(b.toggle(ItemKey{"minecraft:i2", 0})); // removing still works
    EXPECT_FALSE(b.full());
    b.setMaxCount(1);
    EXPECT_EQ(b.size(), 1u);
    EXPECT_EQ(b.items().front().id, "minecraft:i1");
    b.setMaxCount(0);
    EXPECT_EQ(b.maxCount(), 1u);

    Bookmarks limited;
    limited.setMaxCount(2);
    ASSERT_TRUE(limited.fromJsonText(R"(["minecraft:a","minecraft:b","minecraft:c"])"));
    EXPECT_EQ(limited.size(), 3u); // loading keeps everything
    EXPECT_TRUE(limited.full());
    limited.add(ItemKey{"minecraft:d", 0});
    EXPECT_EQ(limited.size(), 3u);
}

TEST(Bookmarks, RobustLoad) {
    Bookmarks b;
    ASSERT_TRUE(b.fromJsonText(R"({"version":1,"items":[
        "minecraft:stone", 42, null, "", ":bad", "minecraft:wool:x", {"id":"minecraft:dirt"},
        "minecraft:wool:3", "stone", "minecraft:stone", "minecraft:planks:*", "minecraft:wool:3"
    ]})"));
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b.items()[0], (ItemKey{"minecraft:stone", 0}));
    EXPECT_EQ(b.items()[1], (ItemKey{"minecraft:wool", 3}));
    EXPECT_EQ(b.items()[2], (ItemKey{"minecraft:planks", ItemKey::ANY_AUX}));

    EXPECT_TRUE(b.fromJsonText(R"(["minecraft:a"])")); // bare array
    EXPECT_EQ(b.size(), 1u);
    EXPECT_TRUE(b.fromJsonText(R"({"items":[]})"));
    EXPECT_TRUE(b.empty());

    for (char const* bad : {"", "{", "null", "42", R"({"items":"minecraft:stone"})", R"({"version":1})"}) {
        b.add(ItemKey{"minecraft:keep", 0});
        EXPECT_FALSE(b.fromJsonText(bad)) << bad;
        EXPECT_TRUE(b.empty()) << bad;
    }

    auto file = tempFile("bookmarks-corrupt.json");
    writeRaw(file, "{\"items\": [\"minecraft:sto");
    Bookmarks c;
    c.add(ItemKey{"minecraft:keep", 0});
    EXPECT_FALSE(c.load(file));
    EXPECT_TRUE(c.empty());
    EXPECT_FALSE(c.load(tempFile("bookmarks-missing.json")));
}

TEST(Bookmarks, AtomicSave) {
    auto dir  = tempFile("atomic-bm");
    auto file = dir / "sub" / "bookmarks.json";
    std::filesystem::remove_all(dir);
    Bookmarks b;
    b.add(ItemKey{"minecraft:stone", 0});
    ASSERT_TRUE(b.save(file)); // creates directories
    b.add(ItemKey{"minecraft:dirt", 0});
    ASSERT_TRUE(b.save(file)); // replaces existing file
    EXPECT_FALSE(std::filesystem::exists(file.string() + ".tmp"));
    Bookmarks loaded;
    ASSERT_TRUE(loaded.load(file));
    EXPECT_EQ(loaded.items(), b.items());

    // A directory in the way of the target: save fails and leaves things as they were.
    auto blocked = dir / "blocked.json";
    std::filesystem::create_directories(blocked / "child");
    EXPECT_FALSE(b.save(blocked));
    EXPECT_TRUE(std::filesystem::is_directory(blocked));
    EXPECT_FALSE(std::filesystem::exists(blocked.string() + ".tmp"));
    std::filesystem::remove_all(dir);
}

TEST(AtomicWrite, ReplacesContent) {
    auto file = tempFile("atomic.txt");
    ASSERT_TRUE(writeFileAtomic(file, "first"));
    EXPECT_EQ(readRaw(file), "first");
    ASSERT_TRUE(writeFileAtomic(file, "second, longer"));
    EXPECT_EQ(readRaw(file), "second, longer");
    ASSERT_TRUE(writeFileAtomic(file, ""));
    EXPECT_EQ(readRaw(file), "");
    EXPECT_FALSE(writeFileAtomic({}, "x"));
    EXPECT_FALSE(readFile(tempFile("atomic-missing.txt")));
}

// ---------------------------------------------------------------- Config

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

TEST(Config, AllFieldsRoundTrip) {
    Config c;
    c.overlayEnabled      = false;
    c.bookmarksEnabled    = false;
    c.cheatMode           = true;
    c.showOnAllContainers = false;
    c.centerSearchBar     = true;
    c.maxColumns          = 7;
    c.lastSearch          = "@minecraft 橡木 \"quoted\"\n";
    c.cheatGiveStackSize  = 16;
    c.showItemIds         = true;
    c.showModName         = false;
    c.bookmarksAddToFront = false;
    c.rememberPage        = true;
    c.lastPage            = 12;

    auto file = tempFile("config-all.json");
    ASSERT_TRUE(c.save(file));
    EXPECT_FALSE(std::filesystem::exists(file.string() + ".tmp"));
    Config l;
    ASSERT_TRUE(l.load(file));
    EXPECT_EQ(l.version, Config::CURRENT_VERSION);
    EXPECT_EQ(l.overlayEnabled, c.overlayEnabled);
    EXPECT_EQ(l.bookmarksEnabled, c.bookmarksEnabled);
    EXPECT_EQ(l.cheatMode, c.cheatMode);
    EXPECT_EQ(l.showOnAllContainers, c.showOnAllContainers);
    EXPECT_EQ(l.centerSearchBar, c.centerSearchBar);
    EXPECT_EQ(l.maxColumns, c.maxColumns);
    EXPECT_EQ(l.lastSearch, c.lastSearch);
    EXPECT_EQ(l.cheatGiveStackSize, c.cheatGiveStackSize);
    EXPECT_EQ(l.showItemIds, c.showItemIds);
    EXPECT_EQ(l.showModName, c.showModName);
    EXPECT_EQ(l.bookmarksAddToFront, c.bookmarksAddToFront);
    EXPECT_EQ(l.rememberPage, c.rememberPage);
    EXPECT_EQ(l.lastPage, c.lastPage);
    EXPECT_EQ(l.toJsonText(), c.toJsonText());
}

TEST(Config, DefaultsForNewFields) {
    Config c;
    ASSERT_TRUE(c.fromJsonText(R"({"version":1,"cheatMode":true})"));
    EXPECT_TRUE(c.cheatMode);
    EXPECT_EQ(c.cheatGiveStackSize, 64);
    EXPECT_FALSE(c.showItemIds);
    EXPECT_TRUE(c.showModName);
    EXPECT_TRUE(c.bookmarksAddToFront);
    EXPECT_EQ(c.lastPage, 0);
}

TEST(Config, WrongTypesKeepDefaults) {
    Config c;
    ASSERT_TRUE(c.fromJsonText(R"({
        "overlayEnabled": "no", "bookmarksEnabled": 0, "cheatMode": 1, "showOnAllContainers": null,
        "centerSearchBar": [], "maxColumns": true, "lastSearch": 5, "cheatGiveStackSize": "32",
        "showItemIds": {}, "lastPage": 2.5, "unknownField": 1
    })"));
    Config d;
    EXPECT_EQ(c.overlayEnabled, d.overlayEnabled);
    EXPECT_EQ(c.bookmarksEnabled, d.bookmarksEnabled);
    EXPECT_EQ(c.cheatMode, d.cheatMode);
    EXPECT_EQ(c.showOnAllContainers, d.showOnAllContainers);
    EXPECT_EQ(c.centerSearchBar, d.centerSearchBar);
    EXPECT_EQ(c.maxColumns, d.maxColumns);
    EXPECT_EQ(c.lastSearch, d.lastSearch);
    EXPECT_EQ(c.cheatGiveStackSize, d.cheatGiveStackSize);
    EXPECT_EQ(c.showItemIds, d.showItemIds);
    EXPECT_EQ(c.lastPage, d.lastPage);
}

TEST(Config, NumericRangesClamped) {
    Config c;
    ASSERT_TRUE(c.fromJsonText(R"({"maxColumns": 1000, "cheatGiveStackSize": 0, "lastPage": -3, "version": 1})"));
    EXPECT_EQ(c.maxColumns, Config::MAX_COLUMNS);
    EXPECT_EQ(c.cheatGiveStackSize, 1);
    EXPECT_EQ(c.lastPage, 0);
    ASSERT_TRUE(c.fromJsonText(R"({"maxColumns": -5, "cheatGiveStackSize": 99999999999})"));
    EXPECT_EQ(c.maxColumns, 0);
    EXPECT_EQ(c.cheatGiveStackSize, 64); // out of int range: default kept
    ASSERT_TRUE(c.fromJsonText(R"({"maxColumns": 9.0, "cheatGiveStackSize": 18446744073709551615})"));
    EXPECT_EQ(c.maxColumns, 9); // integral float accepted
    EXPECT_EQ(c.cheatGiveStackSize, 64);

    Config s;
    s.maxColumns         = -1;
    s.cheatGiveStackSize = 500;
    Config r;
    ASSERT_TRUE(r.fromJsonText(s.toJsonText())); // save sanitizes too
    EXPECT_EQ(r.maxColumns, 0);
    EXPECT_EQ(r.cheatGiveStackSize, 64);
}

TEST(Config, LastSearchTruncatedOnUtf8Boundary) {
    Config c;
    c.lastSearch.clear();
    for (std::size_t i = 0; i < Config::MAX_SEARCH_LENGTH; ++i) c.lastSearch += "橡"; // 3 bytes each
    c.sanitize();
    EXPECT_LE(c.lastSearch.size(), Config::MAX_SEARCH_LENGTH);
    EXPECT_EQ(c.lastSearch.size() % 3, 0u);

    Config bad;
    bad.lastSearch = std::string("ab\xE6", 3); // truncated UTF-8 from a text box
    Config r;
    EXPECT_TRUE(r.fromJsonText(bad.toJsonText()));
    EXPECT_EQ(r.lastSearch.substr(0, 2), "ab");
}

TEST(Config, CorruptAndMigration) {
    Config c;
    c.cheatMode = true;
    for (char const* bad : {"", "{", "[]", "\"x\"", "{\"cheatMode\": tru"}) {
        EXPECT_FALSE(c.fromJsonText(bad)) << bad;
        EXPECT_FALSE(c.cheatMode) << bad; // reset to defaults
    }
    // No version field (pre-release file) and an alias name for maxColumns.
    ASSERT_TRUE(c.fromJsonText(R"({"itemPanelColumns": 6, "cheatMode": true})"));
    EXPECT_EQ(c.maxColumns, 6);
    EXPECT_TRUE(c.cheatMode);
    EXPECT_EQ(c.version, Config::CURRENT_VERSION);
    // The canonical name wins over an alias.
    ASSERT_TRUE(c.fromJsonText(R"({"itemPanelColumns": 6, "maxColumns": 3})"));
    EXPECT_EQ(c.maxColumns, 3);
    // Files from a newer version still load what we understand.
    ASSERT_TRUE(c.fromJsonText(R"({"version": 99, "cheatMode": true, "futureThing": {"a": [1, 2]}})"));
    EXPECT_TRUE(c.cheatMode);
    EXPECT_EQ(c.version, 99);
    // ...and saving keeps the newer version and the fields we don't understand.
    Config again;
    ASSERT_TRUE(again.fromJsonText(c.toJsonText()));
    EXPECT_EQ(again.version, 99);
    EXPECT_EQ(again.extraFields, c.extraFields);
    EXPECT_NE(c.toJsonText().find("futureThing"), std::string::npos);
    // Aliases are consumed, not preserved.
    ASSERT_TRUE(c.fromJsonText(R"({"itemPanelColumns": 6})"));
    EXPECT_TRUE(c.extraFields.empty());
    EXPECT_EQ(c.toJsonText().find("itemPanelColumns"), std::string::npos);
    // Known fields in extraFields never override real values.
    Config clash;
    clash.cheatMode   = true;
    clash.extraFields = R"({"cheatMode": false, "other": 1})";
    Config r;
    ASSERT_TRUE(r.fromJsonText(clash.toJsonText()));
    EXPECT_TRUE(r.cheatMode);
    EXPECT_EQ(r.extraFields, R"({"other":1})");

    auto file = tempFile("config-corrupt.json");
    writeRaw(file, "\xEF\xBB\xBF{\"cheatMode\": true}"); // UTF-8 BOM (Notepad)
    Config bom;
    EXPECT_TRUE(bom.load(file));
    EXPECT_TRUE(bom.cheatMode);
}
