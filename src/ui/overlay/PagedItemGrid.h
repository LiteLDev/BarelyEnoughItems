#pragma once

#include "core/model/ItemKey.h"
#include "core/state/Layout.h"
#include "ui/UiLayer.h"
#include "ui/widgets/Button.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace bei::ui {

class Painter;

/// Internal building block shared by the item list and bookmark panels: a page header
/// ("<  page/total  >") on top of a grid of item cells, with hover highlight, paging by
/// buttons and wheel, and the JEI click behaviour (see activateItem).
///
/// The owner supplies the items through `keyAt` (index -> key, nullptr when out of range)
/// and the item count through setItemCount() every frame; nothing here allocates per frame.
class PagedItemGrid {
public:
    static constexpr float CELL     = 18;
    static constexpr float HEADER_H = 14;
    static constexpr float GAP      = 2;
    static constexpr int   MIN_COLS = 3; ///< Narrower panels are not drawn at all.

    /// Item at list index `i`, or nullptr. Set once by the owner.
    std::function<core::ItemKey const*(std::size_t i)> keyAt;

    /// Fit header + grid into `area`, keeping `bottomReserve` units free at the bottom
    /// (e.g. for a search bar). Returns false (and hides the grid) if it does not fit.
    bool layout(core::Rect const& area, float bottomReserve, int maxColumns);
    /// Update the number of items; keeps the current page when possible.
    void setItemCount(std::size_t count);
    void resetPage() noexcept { mPages.page = 0; }
    void step(int delta) noexcept { mPages.step(delta); }

    /// Draw header and visible cells. Does not draw the tooltip (see hoveredItem()).
    void render(Painter& painter, FrameInfo const& frame, std::string const& emptyText);

    /// Log the layout (as fractions of the screen, for the e2e tests) when it changed.
    void logLayoutIfChanged(char const* panelName, core::Rect const& screen, core::Rect const& extra = {});

    /// Handles header buttons and item clicks. Returns true if the event was consumed.
    bool onMouseButton(MouseButton button, bool down, float x, float y);
    bool onScroll(float x, float y, float delta);

    [[nodiscard]] bool                         shown() const noexcept { return mShown; }
    [[nodiscard]] bool                         contains(float x, float y) const noexcept;
    [[nodiscard]] std::optional<core::ItemKey> itemAt(float x, float y) const;
    /// Bounds of everything drawn (header + grid).
    [[nodiscard]] core::Rect const&            bounds() const noexcept { return mBounds; }
    [[nodiscard]] core::GridLayout const&      grid() const noexcept { return mGrid; }
    [[nodiscard]] core::Pagination const&      pages() const noexcept { return mPages; }

private:
    [[nodiscard]] core::ItemKey const* cellKey(int cell) const;

    core::GridLayout mGrid;
    core::Pagination mPages;
    core::Rect       mBounds;
    Button           mPrev;
    Button           mNext;
    bool             mShown = false;
    std::string      mPageLabel;
    int              mLabelPage  = -1;
    int              mLabelCount = -1;
    core::Rect       mLoggedBounds;
    core::Rect       mLoggedScreen;
    core::Rect       mLoggedExtra;
};

/// JEI click behaviour on an item in a BEI panel: in cheat mode (Config::cheatMode and
/// Actions::canCheat()) a left click gives a full stack (shift: one item); otherwise left click
/// shows recipes. Right click shows uses. Returns true if the button is one BEI handles.
bool activateItem(core::ItemKey const& item, MouseButton button);

/// True if clicks on panel items give items (Config::cheatMode and Actions::canCheat()).
[[nodiscard]] bool cheatActive();

/// Shared visibility rule of the overlay panels: in a level, on a container screen
/// (all of them or only inventory/crafting, per Config::showOnAllContainers), overlay enabled
/// and the recipe screen closed.
[[nodiscard]] bool overlayAllowed(FrameInfo const& frame);

/// Ask for the item tooltip of a hovered panel item. It is drawn by a dedicated layer above
/// all overlay panels at the end of the frame, so no panel can paint over it.
void requestItemTooltip(core::ItemKey const& item);

/// True when `frame` is the first rendered frame after the overlay was hidden (screen opened).
/// `lastTime` is per caller state.
[[nodiscard]] bool reopened(FrameInfo const& frame, double& lastTime);

} // namespace bei::ui
