#pragma once

#include <algorithm>

namespace bei::core {

/// Axis-aligned rectangle in GUI units (the game's scaled UI coordinate space).
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;

    [[nodiscard]] float right() const noexcept { return x + w; }
    [[nodiscard]] float bottom() const noexcept { return y + h; }
    [[nodiscard]] bool  contains(float px, float py) const noexcept {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    [[nodiscard]] bool empty() const noexcept { return w <= 0 || h <= 0; }
};

/// A grid of square cells fitted into an area.
struct GridLayout {
    Rect  bounds;   ///< Area actually used by the cells (left/top aligned inside the input area).
    float cell = 18; ///< Cell size.
    int   cols = 0;
    int   rows = 0;

    [[nodiscard]] int  capacity() const noexcept { return cols * rows; }
    [[nodiscard]] Rect cellRect(int index) const noexcept {
        return {bounds.x + static_cast<float>(index % std::max(cols, 1)) * cell,
                bounds.y + static_cast<float>(index / std::max(cols, 1)) * cell,
                cell,
                cell};
    }
    /// Cell index at a point, or -1.
    [[nodiscard]] int hitTest(float px, float py) const noexcept;
};

/// Fit as many `cell`-sized cells as possible into `area`, centered horizontally.
[[nodiscard]] GridLayout fitGrid(Rect const& area, float cell, int maxCols = 0);

/// Page arithmetic for a list of `itemCount` items shown `perPage` at a time.
struct Pagination {
    int itemCount = 0;
    int perPage   = 1;
    int page      = 0;

    [[nodiscard]] int pageCount() const noexcept;
    [[nodiscard]] int firstIndex() const noexcept { return page * perPage; }
    /// Clamp page into [0, pageCount-1].
    void              clamp() noexcept;
    /// Move by delta pages, wrapping around at both ends.
    void              step(int delta) noexcept;
};

/// Screen space layout for the overlay, computed from the screen size and the vanilla
/// container panel rectangle (both GUI units).
struct OverlayLayout {
    Rect itemPanel;     ///< Right side: search box + item grid + paging.
    Rect bookmarkPanel; ///< Left side.
};

/// Compute panels from the free space left/right of the container rectangle.
/// `margin` is kept free around panels. If the container rect is empty, the panels use
/// the right/left 1/4 of the screen.
[[nodiscard]] OverlayLayout computeOverlayLayout(float screenW, float screenH, Rect const& container, float margin = 4);

} // namespace bei::core
