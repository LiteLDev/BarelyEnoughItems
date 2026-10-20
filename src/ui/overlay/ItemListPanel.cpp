// Item list overlay: page header, item grid and search bar to the right of container screens.
#include "ui/overlay/ItemListPanel.h"

#include "app/Controller.h"
#include "app/I18n.h"
#include "core/search/SearchEngine.h"
#include "core/state/Layout.h"
#include "game/items/ItemCatalog.h"
#include "ui/UiRoot.h"
#include "ui/gfx/Painter.h"
#include "ui/overlay/PagedItemGrid.h"
#include "ui/widgets/TextBox.h"

#include <vector>

namespace bei::ui {

namespace {
struct State {
    core::SearchEngine       search;
    std::vector<std::size_t> filtered; ///< Indices into ItemCatalog::entries().
    std::uint64_t            filteredGen = ~0ull;
    std::string              filteredQuery;
    bool                     filterValid = false;

    PagedItemGrid grid;
    TextBox       searchBox;
    bool          searchShown = false;
    double        lastRender  = -1;
    std::string   emptyText;
    bool          initialized = false;
};
State gState;

void initOnce() {
    if (gState.initialized) return;
    gState.initialized = true;
    gState.grid.keyAt  = [](std::size_t i) -> core::ItemKey const* {
        auto const& entries = game::ItemCatalog::get().entries();
        if (i >= gState.filtered.size()) return nullptr;
        auto idx = gState.filtered[i];
        return idx < entries.size() ? &entries[idx].key : nullptr;
    };
    gState.searchBox.setText(app::Controller::get().searchText());
    gState.searchBox.onChange = [](std::string const& t) { app::Controller::get().setSearchText(t); };
}

/// Recompute the filtered list only when the catalog or the query changed.
void refreshFilter() {
    auto& catalog = game::ItemCatalog::get();
    auto& query   = app::Controller::get().searchText();
    // Keep the box in sync if someone else changed the search text.
    if (gState.searchBox.text() != query) gState.searchBox.setText(query);
    bool genChanged   = gState.filteredGen != catalog.generation();
    bool queryChanged = gState.filteredQuery != query;
    if (gState.filterValid && !genChanged && !queryChanged) return;
    // With the generation, the engine skips its per-entry change check until the catalog is rebuilt.
    gState.search.setItems(&catalog.entries(), catalog.generation());
    gState.filtered      = gState.search.filter(query);
    gState.filteredGen   = catalog.generation();
    gState.filteredQuery = query;
    if (queryChanged && gState.filterValid) gState.grid.resetPage(); // new query: back to page 1
    gState.filterValid = true;
}
} // namespace

ItemListPanel& ItemListPanel::get() {
    static ItemListPanel instance;
    return instance;
}

bool ItemListPanel::visible(FrameInfo const& frame) const { return overlayAllowed(frame); }

void ItemListPanel::render(Painter& p, FrameInfo const& frame) {
    initOnce();
    if (reopened(frame, gState.lastRender)) {
        // The screen was (re)opened: start unfocused and pick up language changes.
        gState.searchBox.setFocused(false);
        gState.searchBox.placeholder = app::tr("bei.search.placeholder", "Search...");
        gState.emptyText             = app::tr("bei.overlay.no_items", "No matching items");
    }
    refreshFilter();

    auto layout = overlayLayout(frame);
    gState.searchShown =
        layout.itemPanelUsable && !layout.searchBar.empty()
        && gState.grid.layout(layout.itemPageControls, layout.itemGrid, app::Controller::get().config().maxColumns);
    if (!gState.searchShown) {
        gState.searchBox.setFocused(false);
        return; // too narrow: draw nothing
    }
    // Search bar: under the container (Config::centerSearchBar, when it fits) or at the bottom of the
    // panel, narrowed to the grid columns there.
    gState.searchBox.rect = layout.searchBar;
    if (!layout.searchBarCentered) {
        auto const& grid      = gState.grid.grid().bounds;
        gState.searchBox.rect = {grid.x, layout.searchBar.y, grid.w, layout.searchBar.h};
    }
    gState.grid.logLayoutIfChanged("Item panel", frame.screen, gState.searchBox.rect);

    auto& catalog = game::ItemCatalog::get();
    gState.grid.setItemCount(gState.filtered.size());
    gState.grid.render(p, frame, catalog.entries().empty() ? std::string{} : gState.emptyText);
    gState.searchBox.render(p, frame.time);

    if (auto item = gState.grid.itemAt(frame.mouseX, frame.mouseY)) requestItemTooltip(*item);
}

std::optional<core::ItemKey> ItemListPanel::itemAt(float x, float y) const { return gState.grid.itemAt(x, y); }

bool ItemListPanel::contains(float x, float y) const {
    return gState.grid.contains(x, y) || (gState.searchShown && gState.searchBox.rect.contains(x, y));
}

bool ItemListPanel::onMouseButton(MouseButton button, bool down, float x, float y) {
    // Also unfocuses the box when clicking elsewhere.
    if (gState.searchShown && gState.searchBox.onMouseButton(button == MouseButton::Right, down, x, y)) return true;
    return gState.grid.onMouseButton(button, down, x, y);
}

bool ItemListPanel::onScroll(float x, float y, float delta) { return gState.grid.onScroll(x, y, delta); }

bool ItemListPanel::onKey(int vk, bool down) { return gState.searchBox.onKey(vk, down); }

bool ItemListPanel::onChar(std::string const& utf8) { return gState.searchBox.onChar(utf8); }

bool ItemListPanel::wantsTextInput() const { return gState.searchShown && gState.searchBox.focused(); }

BEI_REGISTER_UI_LAYER(ItemListPanel::get());

} // namespace bei::ui
