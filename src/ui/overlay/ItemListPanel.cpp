// Scaffold implementation: search box + paged item grid on the right of container screens.
// Unit "Item list overlay" adds the bookmark panel, page buttons, cheat mode and polish.
#include "ui/overlay/ItemListPanel.h"

#include "app/Controller.h"
#include "core/search/SearchEngine.h"
#include "core/state/Layout.h"
#include "game/items/ItemCatalog.h"
#include "ui/UiRoot.h"
#include "ui/gfx/Painter.h"
#include "ui/recipe/RecipeScreen.h"
#include "ui/widgets/TextBox.h"
#include "ui/widgets/Tooltip.h"

#include <vector>

namespace bei::ui {

namespace {
constexpr float CELL     = 18;
constexpr float SEARCH_H = 18;

struct State {
    core::SearchEngine       search;
    std::vector<std::size_t> filtered;
    std::uint64_t            filteredGen = ~0ull;
    std::string              filteredQuery;
    core::Pagination         pages;
    core::Rect               panel;
    core::GridLayout         grid;
    TextBox                  searchBox;
    bool                     initialized = false;
};
State gState;

void refreshFilter() {
    auto& catalog = game::ItemCatalog::get();
    auto& query   = app::Controller::get().searchText();
    if (gState.filteredGen == catalog.generation() && gState.filteredQuery == query) return;
    gState.search.setItems(&catalog.entries());
    gState.filtered      = gState.search.filter(query);
    gState.filteredGen   = catalog.generation();
    gState.filteredQuery = query;
}
} // namespace

ItemListPanel& ItemListPanel::get() {
    static ItemListPanel instance;
    return instance;
}

bool ItemListPanel::visible(FrameInfo const& frame) const {
    return frame.inLevel && frame.isContainerScreen && app::Controller::get().config().overlayEnabled;
}

void ItemListPanel::render(Painter& p, FrameInfo const& frame) {
    if (!gState.initialized) {
        gState.searchBox.placeholder = "Search...";
        gState.searchBox.setText(app::Controller::get().searchText());
        gState.searchBox.onChange = [](std::string const& t) { app::Controller::get().setSearchText(t); };
        gState.initialized        = true;
    }
    refreshFilter();

    auto layout  = core::computeOverlayLayout(frame.screen.w, frame.screen.h, frame.container);
    gState.panel = layout.itemPanel;
    if (gState.panel.w < CELL * 2) return;

    // Search box at the bottom, paging header at the top, grid in between.
    gState.searchBox.rect = {gState.panel.x, gState.panel.bottom() - SEARCH_H, gState.panel.w, SEARCH_H};
    core::Rect gridArea{gState.panel.x, gState.panel.y + 12, gState.panel.w, gState.panel.h - 12 - SEARCH_H - 2};
    gState.grid = core::fitGrid(gridArea, CELL, app::Controller::get().config().maxColumns);

    gState.pages.itemCount = static_cast<int>(gState.filtered.size());
    gState.pages.perPage   = std::max(1, gState.grid.capacity());
    gState.pages.clamp();

    auto& entries = game::ItemCatalog::get().entries();
    p.text(
        std::to_string(gState.pages.page + 1) + "/" + std::to_string(gState.pages.pageCount()),
        gState.panel.x + gState.panel.w / 2,
        gState.panel.y + 2,
        palette::TEXT_LIGHT,
        1,
        TextAlign::Center
    );

    int hovered = gState.grid.hitTest(frame.mouseX, frame.mouseY);
    for (int i = 0; i < gState.grid.capacity(); ++i) {
        auto idx = static_cast<std::size_t>(gState.pages.firstIndex() + i);
        if (idx >= gState.filtered.size()) break;
        auto cell = gState.grid.cellRect(i);
        if (i == hovered) p.fillRect(cell, palette::HOVER, Painter::Layer::Foreground);
        p.item(entries[gState.filtered[idx]].key, cell.x + 1, cell.y + 1);
    }
    gState.searchBox.render(p, frame.time);

    if (auto item = itemAt(frame.mouseX, frame.mouseY)) {
        p.flush();
        drawItemTooltip(p, *item, frame.mouseX, frame.mouseY, frame);
    }
}

std::optional<core::ItemKey> ItemListPanel::itemAt(float x, float y) const {
    int i = gState.grid.hitTest(x, y);
    if (i < 0) return std::nullopt;
    auto idx = static_cast<std::size_t>(gState.pages.firstIndex() + i);
    if (idx >= gState.filtered.size()) return std::nullopt;
    auto& entries = game::ItemCatalog::get().entries();
    if (gState.filtered[idx] >= entries.size()) return std::nullopt;
    return entries[gState.filtered[idx]].key;
}

bool ItemListPanel::contains(float x, float y) const { return gState.panel.contains(x, y); }

bool ItemListPanel::onMouseButton(MouseButton button, bool down, float x, float y) {
    if (gState.searchBox.onMouseButton(button == MouseButton::Right, down, x, y)) return true;
    if (!contains(x, y)) return false;
    if (down) {
        if (auto item = itemAt(x, y)) {
            if (button == MouseButton::Left) app::Controller::get().showRecipes(*item);
            if (button == MouseButton::Right) app::Controller::get().showUses(*item);
        }
    }
    return true;
}

bool ItemListPanel::onScroll(float x, float y, float delta) {
    if (!contains(x, y)) return false;
    gState.pages.step(delta > 0 ? -1 : 1);
    return true;
}

bool ItemListPanel::onKey(int vk, bool down) { return gState.searchBox.onKey(vk, down); }

bool ItemListPanel::onChar(std::string const& utf8) { return gState.searchBox.onChar(utf8); }

bool ItemListPanel::wantsTextInput() const { return gState.searchBox.focused(); }

BEI_REGISTER_UI_LAYER(ItemListPanel::get());

} // namespace bei::ui
