#include "core/state/Layout.h"

#include <cmath>

namespace bei::core {

int GridLayout::hitTest(float px, float py) const noexcept {
    if (cols <= 0 || rows <= 0 || !bounds.contains(px, py)) return -1;
    int c = static_cast<int>((px - bounds.x) / cell);
    int r = static_cast<int>((py - bounds.y) / cell);
    if (c < 0 || c >= cols || r < 0 || r >= rows) return -1;
    return r * cols + c;
}

GridLayout fitGrid(Rect const& area, float cell, int maxCols) {
    GridLayout g;
    g.cell = cell;
    if (cell <= 0 || area.empty()) return g;
    g.cols = static_cast<int>(std::floor(area.w / cell));
    g.rows = static_cast<int>(std::floor(area.h / cell));
    if (maxCols > 0) g.cols = std::min(g.cols, maxCols);
    g.cols   = std::max(g.cols, 0);
    g.rows   = std::max(g.rows, 0);
    float w  = static_cast<float>(g.cols) * cell;
    float h  = static_cast<float>(g.rows) * cell;
    g.bounds = {area.x + (area.w - w) / 2, area.y, w, h};
    return g;
}

int Pagination::pageCount() const noexcept {
    if (perPage <= 0) return 1;
    return std::max(1, (itemCount + perPage - 1) / perPage);
}

void Pagination::clamp() noexcept { page = std::clamp(page, 0, pageCount() - 1); }

void Pagination::step(int delta) noexcept {
    int n = pageCount();
    page  = ((page + delta) % n + n) % n;
}

OverlayLayout computeOverlayLayout(float screenW, float screenH, Rect const& container, float margin) {
    OverlayLayout out;
    if (container.empty()) {
        out.itemPanel     = {screenW * 0.75f + margin, margin, screenW * 0.25f - 2 * margin, screenH - 2 * margin};
        out.bookmarkPanel = {margin, margin, screenW * 0.25f - 2 * margin, screenH - 2 * margin};
        return out;
    }
    float rightX      = container.right() + margin;
    out.itemPanel     = {rightX, margin, std::max(0.f, screenW - rightX - margin), screenH - 2 * margin};
    out.bookmarkPanel = {margin, margin, std::max(0.f, container.x - 2 * margin), screenH - 2 * margin};
    return out;
}

} // namespace bei::core
