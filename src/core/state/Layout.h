#pragma once

#include <algorithm>

namespace bei::core {

/// Axis-aligned rectangle in GUI units (the game's scaled UI coordinate space).
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;

    [[nodiscard]] float right() const noexcept { return x + w; }
    [[nodiscard]] float bottom() const noexcept { return y + h; }
    [[nodiscard]] float centerX() const noexcept { return x + w / 2; }
    [[nodiscard]] float centerY() const noexcept { return y + h / 2; }
    [[nodiscard]] bool  contains(float px, float py) const noexcept {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    [[nodiscard]] bool empty() const noexcept { return !(w > 0) || !(h > 0); }
    /// Shrink by `d` on every side (never below zero size; the result stays centered).
    [[nodiscard]] Rect inset(float d) const noexcept {
        float nw = std::max(0.f, w - 2 * d);
        float nh = std::max(0.f, h - 2 * d);
        return {x + (w - nw) / 2, y + (h - nh) / 2, nw, nh};
    }
    /// Overlap of two rectangles (zero-sized if they don't overlap).
    [[nodiscard]] Rect intersect(Rect const& o) const noexcept {
        float l = std::max(x, o.x), t = std::max(y, o.y);
        float r = std::min(right(), o.right()), b = std::min(bottom(), o.bottom());
        return {l, t, std::max(0.f, r - l), std::max(0.f, b - t)};
    }

    friend bool operator==(Rect const&, Rect const&) = default;
};

/// A grid of square cells fitted into an area.
struct GridLayout {
    Rect  bounds;    ///< Area actually used by the cells (centered horizontally inside the input area).
    float cell = 18; ///< Cell size.
    int   cols = 0;
    int   rows = 0;

    [[nodiscard]] int  capacity() const noexcept { return cols * rows; }
    [[nodiscard]] Rect cellRect(int index) const noexcept {
        return {
            bounds.x + static_cast<float>(index % std::max(cols, 1)) * cell,
            bounds.y + static_cast<float>(index / std::max(cols, 1)) * cell,
            cell,
            cell
        };
    }
    /// Cell index at a point, or -1. Right/bottom edges are exclusive.
    [[nodiscard]] int hitTest(float px, float py) const noexcept;
    /// Like hitTest, but also returns -1 for cells at or past `count` (a partially filled last page).
    [[nodiscard]] int hitTest(float px, float py, int count) const noexcept;
};

/// Fit as many `cell`-sized cells as possible into `area`, centered horizontally (and vertically
/// if `centerVertically`), otherwise top aligned. `maxCols` <= 0 means no column limit.
[[nodiscard]] GridLayout fitGrid(Rect const& area, float cell, int maxCols = 0, bool centerVertically = false);

/// Page arithmetic for a list of `itemCount` items shown `perPage` at a time.
/// `itemCount == 0` still has one (empty) page.
struct Pagination {
    int itemCount = 0;
    int perPage   = 1;
    int page      = 0;

    [[nodiscard]] int pageCount() const noexcept;
    [[nodiscard]] int firstIndex() const noexcept { return std::max(page, 0) * std::max(perPage, 0); }
    /// Number of items on the current page (0..perPage).
    [[nodiscard]] int visibleCount() const noexcept;
    /// Page that contains item `index` (clamped into the valid page range).
    [[nodiscard]] int  pageOf(int index) const noexcept;
    [[nodiscard]] bool isFirstPage() const noexcept { return page <= 0; }
    [[nodiscard]] bool isLastPage() const noexcept { return page >= pageCount() - 1; }
    /// Clamp page into [0, pageCount-1].
    void clamp() noexcept;
    /// Move by delta pages, wrapping around at both ends.
    void step(int delta) noexcept;
    /// Jump to a page (clamped, no wrapping).
    void jumpTo(int newPage) noexcept;
    /// Jump to the page containing item `index`.
    void showIndex(int index) noexcept { page = pageOf(index); }
    /// Change item count and/or page size, keeping the first visible item on screen if possible
    /// (the new page is the one containing the old first visible item, then clamped).
    void update(int newItemCount, int newPerPage) noexcept;
};

/// Inputs for computeOverlayLayout beyond the screen and container rectangles.
struct OverlayLayoutOptions {
    /// Panels narrower than this are not usable (OverlayLayout::*Usable == false).
    static constexpr float DEFAULT_MIN_PANEL_WIDTH = 40;

    float margin                  = 4;  ///< Free space kept around panels and between panel parts.
    float cell                    = 18; ///< Item cell size, used to snap the grid areas.
    float searchBarHeight         = 18;
    float pageControlsHeight      = 12; ///< Row with "<  1/3  >" at the top of each panel.
    float minPanelWidth           = DEFAULT_MIN_PANEL_WIDTH;
    bool  centerSearchBar         = false; ///< Put the search bar centered under the container.
    float centerSearchBarMaxWidth = 0;     ///< 0 = container width.
};

/// Screen space layout for the overlay, computed from the screen size and the vanilla
/// container panel rectangle (both GUI units). All rects are clamped to the screen and never
/// have a negative size.
struct OverlayLayout {
    Rect itemPanel;     ///< Right side: search box + item grid + paging.
    Rect bookmarkPanel; ///< Left side.

    // Sub-areas of the item panel (or the centered search bar). Filled by computeOverlayLayout.
    Rect itemPageControls;     ///< Paging row at the top of the item panel.
    Rect itemGrid;             ///< Area for fitGrid() of the item list.
    Rect searchBar;            ///< Search text box (inside the item panel, or under the container).
    Rect bookmarkPageControls; ///< Paging row at the top of the bookmark panel.
    Rect bookmarkGrid;         ///< Area for fitGrid() of the bookmarks.

    bool itemPanelUsable     = false; ///< itemPanel is at least minPanelWidth wide and fits a cell.
    bool bookmarkPanelUsable = false;
    bool searchBarCentered   = false; ///< centerSearchBar was requested and there was room for it.
};

/// Compute panels from the free space left/right of the container rectangle.
/// `margin` is kept free around panels. If the container rect is empty, the panels use
/// the right/left 1/4 of the screen.
[[nodiscard]] OverlayLayout computeOverlayLayout(float screenW, float screenH, Rect const& container, float margin = 4);

/// Full version. The container is clamped to the screen first. If there is not enough room
/// beside the container the panel is marked unusable (its rect may then be zero-sized).
/// With `centerSearchBar`, the search bar is placed under the container when it fits there,
/// otherwise it falls back to the bottom of the item panel.
[[nodiscard]] OverlayLayout
computeOverlayLayout(float screenW, float screenH, Rect const& container, OverlayLayoutOptions const& options);

} // namespace bei::core
