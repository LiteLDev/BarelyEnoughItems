#include "core/state/Layout.h"

#include <cmath>
#include <limits>

namespace bei::core {

namespace {

/// Finite, non-negative value or 0.
float sane(float v) noexcept { return std::isfinite(v) && v > 0 ? v : 0.f; }

/// Rect with non-negative, finite size.
Rect sane(Rect r) noexcept {
    if (!std::isfinite(r.x)) r.x = 0;
    if (!std::isfinite(r.y)) r.y = 0;
    r.w = sane(r.w);
    r.h = sane(r.h);
    return r;
}

/// Snap a rect to whole GUI units, shrinking inwards (left/top rounded up, right/bottom down).
Rect snapInward(Rect r) noexcept {
    float l = std::ceil(r.x), t = std::ceil(r.y);
    float rr = std::floor(r.right()), b = std::floor(r.bottom());
    return {l, t, std::max(0.f, rr - l), std::max(0.f, b - t)};
}

/// Cells per axis are capped so cols * rows always fits an int.
constexpr int MAX_CELLS_PER_AXIS = 1 << 15;

/// Cells along an extent, tolerant to float error (89.99999 / 18 still gives 5).
int cellsAlong(float extent, float cell) noexcept {
    double n = std::floor(static_cast<double>(extent) / cell + 1e-4);
    if (!(n > 0)) return 0;
    return n > MAX_CELLS_PER_AXIS ? MAX_CELLS_PER_AXIS : static_cast<int>(n);
}

void layoutItemPanel(
    OverlayLayout&              out,
    Rect const&                 container,
    float                       screenW,
    float                       screenH,
    OverlayLayoutOptions const& o,
    float                       m
) {
    Rect  p              = out.itemPanel;
    float gap            = std::min(m, 2.f);
    float pch            = std::min(sane(o.pageControlsHeight), p.h);
    float sbh            = sane(o.searchBarHeight);
    out.itemPageControls = {p.x, p.y, p.w, pch};

    if (o.centerSearchBar && !container.empty()) {
        float maxW = std::max(0.f, screenW - 2 * m);
        float width =
            sane(o.centerSearchBarMaxWidth) > 0 ? std::min(o.centerSearchBarMaxWidth, container.w) : container.w;
        width   = std::min(width, maxW);
        float x = std::clamp(container.centerX() - width / 2, m, std::max(m, screenW - m - width));
        float y = container.bottom() + m;
        if (width > 0 && sbh > 0 && y + sbh <= screenH - m) {
            out.searchBar         = snapInward({x, y, width, sbh});
            out.searchBarCentered = true;
        }
    }

    float gridTop = p.y + pch + gap;
    if (out.searchBarCentered) {
        out.itemGrid = {p.x, gridTop, p.w, std::max(0.f, p.bottom() - gridTop)};
    } else {
        float bar     = std::min(sbh, std::max(0.f, p.h - pch));
        out.searchBar = {p.x, p.bottom() - bar, p.w, bar};
        out.itemGrid  = {p.x, gridTop, p.w, std::max(0.f, out.searchBar.y - gap - gridTop)};
    }
}

} // namespace

int GridLayout::hitTest(float px, float py) const noexcept {
    if (cols <= 0 || rows <= 0 || !(cell > 0) || !std::isfinite(px) || !std::isfinite(py)) return -1;
    if (!bounds.contains(px, py)) return -1;
    int c = static_cast<int>(std::floor((px - bounds.x) / cell));
    int r = static_cast<int>(std::floor((py - bounds.y) / cell));
    if (c < 0 || c >= cols || r < 0 || r >= rows) return -1;
    return r * cols + c;
}

int GridLayout::hitTest(float px, float py, int count) const noexcept {
    int i = hitTest(px, py);
    return i >= 0 && i < count ? i : -1;
}

GridLayout fitGrid(Rect const& area, float cell, int maxCols, bool centerVertically) {
    GridLayout g;
    g.cell = cell;
    if (!(cell > 0) || !std::isfinite(cell)) return g;
    Rect a = sane(area);
    if (a.empty()) {
        g.bounds = {a.x, a.y, 0, 0};
        return g;
    }
    g.cols = cellsAlong(a.w, cell);
    g.rows = cellsAlong(a.h, cell);
    if (maxCols > 0) g.cols = std::min(g.cols, maxCols);
    if (g.cols == 0 || g.rows == 0) g.cols = g.rows = 0;
    float w  = static_cast<float>(g.cols) * cell;
    float h  = static_cast<float>(g.rows) * cell;
    float x  = a.x + std::max(0.f, (a.w - w) / 2);
    float y  = centerVertically ? a.y + std::max(0.f, (a.h - h) / 2) : a.y;
    g.bounds = {x, y, w, h};
    return g;
}

int Pagination::pageCount() const noexcept {
    if (perPage <= 0 || itemCount <= 0) return 1;
    return 1 + (itemCount - 1) / perPage;
}

int Pagination::visibleCount() const noexcept {
    if (perPage <= 0 || itemCount <= 0) return 0;
    long long first = static_cast<long long>(std::max(page, 0)) * perPage;
    long long rest  = static_cast<long long>(itemCount) - first;
    return static_cast<int>(std::clamp<long long>(rest, 0, perPage));
}

int Pagination::pageOf(int index) const noexcept {
    if (perPage <= 0 || index <= 0) return 0;
    return std::min(index / perPage, pageCount() - 1);
}

void Pagination::clamp() noexcept { page = std::clamp(page, 0, pageCount() - 1); }

void Pagination::step(int delta) noexcept {
    int n = pageCount();
    int p = std::clamp(page, 0, n - 1);
    page  = static_cast<int>(((static_cast<long long>(p) + delta) % n + n) % n);
}

void Pagination::jumpTo(int newPage) noexcept {
    page = newPage;
    clamp();
}

void Pagination::update(int newItemCount, int newPerPage) noexcept {
    clamp();
    int anchor = firstIndex();
    itemCount  = std::max(0, newItemCount);
    perPage    = newPerPage;
    if (anchor >= itemCount) anchor = itemCount - 1;
    page = pageOf(anchor);
}

OverlayLayout computeOverlayLayout(float screenW, float screenH, Rect const& container, float margin) {
    OverlayLayoutOptions o;
    o.margin = margin;
    return computeOverlayLayout(screenW, screenH, container, o);
}

OverlayLayout
computeOverlayLayout(float screenW, float screenH, Rect const& container, OverlayLayoutOptions const& options) {
    OverlayLayout out;
    float         W = sane(screenW), H = sane(screenH);
    float         m      = std::min(sane(options.margin), std::min(W, H) / 4);
    float         panelH = std::max(0.f, H - 2 * m);

    Rect c = sane(container).intersect({0, 0, W, H});
    if (c.empty()) {
        float quarter     = W / 4;
        out.itemPanel     = {W - quarter + m, m, std::max(0.f, quarter - 2 * m), panelH};
        out.bookmarkPanel = {m, m, std::max(0.f, quarter - 2 * m), panelH};
        c                 = {};
    } else {
        float rightX      = c.right() + m;
        out.itemPanel     = {rightX, m, std::max(0.f, W - m - rightX), panelH};
        out.bookmarkPanel = {m, m, std::max(0.f, c.x - 2 * m), panelH};
    }
    out.itemPanel     = snapInward(out.itemPanel);
    out.bookmarkPanel = snapInward(out.bookmarkPanel);

    float cell = sane(options.cell);
    float minW = std::max(sane(options.minPanelWidth), cell);
    float pch  = sane(options.pageControlsHeight);
    // Usable = wide enough and the grid area holds at least one cell.
    auto usable = [&](Rect const& panel, Rect const& grid) {
        return panel.w >= minW && panel.w > 0 && fitGrid(grid, cell).capacity() > 0;
    };

    layoutItemPanel(out, c, W, H, options, m);
    out.itemPanelUsable = usable(out.itemPanel, out.itemGrid);

    Rect  b                  = out.bookmarkPanel;
    float gap                = std::min(m, 2.f);
    float bpch               = std::min(pch, b.h);
    out.bookmarkPageControls = {b.x, b.y, b.w, bpch};
    out.bookmarkGrid         = {b.x, b.y + bpch + gap, b.w, std::max(0.f, b.h - bpch - gap)};
    out.bookmarkPanelUsable  = usable(b, out.bookmarkGrid);
    return out;
}

} // namespace bei::core
