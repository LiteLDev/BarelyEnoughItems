#include "ui/overlay/PagedItemGrid.h"

#include "actions/Actions.h"
#include "app/Controller.h"
#include "game/items/ItemCatalog.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"
#include "ui/overlay/ItemListPanel.h"

#include "mc/world/item/ItemStack.h"

#include <algorithm>

// Avoid <windows.h> here (macro clashes with game headers); only this one call is needed.
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
#pragma comment(lib, "user32")

namespace bei::ui {

namespace {
constexpr float BUTTON_W  = 14;
constexpr int   VK_SHIFT_ = 0x10;
bool shiftDown() { return (GetAsyncKeyState(VK_SHIFT_) & 0x8000) != 0; }
} // namespace

bool cheatActive() { return app::Controller::get().config().cheatMode && actions::Actions::get().canCheat(); }

bool activateItem(core::ItemKey const& item, MouseButton button) {
    auto& controller = app::Controller::get();
    switch (button) {
    case MouseButton::Left:
        if (cheatActive()) {
            int count = 64;
            if (shiftDown()) {
                count = 1;
            } else if (auto const* stack = game::ItemCatalog::get().displayStack(item)) {
                count = std::max<int>(1, stack->getMaxStackSize());
            }
            actions::Actions::get().give(item, count);
        } else {
            controller.showRecipes(item);
        }
        return true;
    case MouseButton::Right: controller.showUses(item); return true;
    default: return false; // middle click is a hotkey (input unit) acting on itemAt()
    }
}

bool PagedItemGrid::layout(core::Rect const& header, core::Rect const& gridArea, int maxColumns) {
    mGrid  = core::fitGrid(gridArea, CELL, maxColumns);
    mShown = mGrid.cols >= MIN_COLS && mGrid.rows >= 1 && !header.empty();
    if (!mShown) {
        mBounds = {};
        return false;
    }
    float left  = mGrid.bounds.x;
    float right = mGrid.bounds.right();
    mPrev.rect  = {left, header.y, BUTTON_W, header.h};
    mNext.rect  = {right - BUTTON_W, header.y, BUTTON_W, header.h};
    if (mPrev.label.empty()) {
        mPrev.label = "<";
        mNext.label = ">";
    }
    mBounds = {left, header.y, right - left, mGrid.bounds.bottom() - header.y};
    mPages.perPage = std::max(1, mGrid.capacity());
    mPages.clamp();
    return true;
}

void PagedItemGrid::setItemCount(std::size_t count) {
    mPages.itemCount = static_cast<int>(std::min<std::size_t>(count, 0x7FFFFFFF));
    mPages.clamp();
}

void PagedItemGrid::logLayoutIfChanged(char const* panelName, core::Rect const& screen, core::Rect const& extra) {
    auto same = [](core::Rect const& a, core::Rect const& b) {
        return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    };
    if (!mShown || screen.empty()) return;
    if (same(mLoggedBounds, mBounds) && same(mLoggedScreen, screen) && same(mLoggedExtra, extra)) return;
    mLoggedBounds = mBounds;
    mLoggedScreen = screen;
    mLoggedExtra  = extra;
    auto fx       = [&](float x) { return x / screen.w; };
    auto fy       = [&](float y) { return y / screen.h; };
    auto cell0    = mGrid.cellRect(0);
    log().info(
        "{} layout: cols={} rows={} cell={:.4f},{:.4f} cell0={:.3f},{:.3f} prev={:.3f},{:.3f} next={:.3f},{:.3f} "
        "extra={:.3f},{:.3f}",
        panelName,
        mGrid.cols,
        mGrid.rows,
        fx(CELL),
        fy(CELL),
        fx(cell0.x + CELL / 2),
        fy(cell0.y + CELL / 2),
        fx(mPrev.rect.x + mPrev.rect.w / 2),
        fy(mPrev.rect.y + mPrev.rect.h / 2),
        fx(mNext.rect.x + mNext.rect.w / 2),
        fy(mNext.rect.y + mNext.rect.h / 2),
        fx(extra.x + extra.w / 2),
        fy(extra.y + extra.h / 2)
    );
}

core::ItemKey const* PagedItemGrid::cellKey(int cell) const {
    if (cell < 0 || cell >= mGrid.capacity() || !keyAt) return nullptr;
    auto idx = static_cast<std::size_t>(mPages.firstIndex()) + static_cast<std::size_t>(cell);
    if (idx >= static_cast<std::size_t>(mPages.itemCount)) return nullptr;
    return keyAt(idx);
}

void PagedItemGrid::render(Painter& p, FrameInfo const& frame, std::string const& emptyText) {
    if (!mShown) return;

    // Header: page buttons and "page/total".
    int pageCount = mPages.pageCount();
    mPrev.enabled = mNext.enabled = pageCount > 1;
    mPrev.render(p, frame.mouseX, frame.mouseY);
    mNext.render(p, frame.mouseX, frame.mouseY);
    if (mLabelPage != mPages.page || mLabelCount != pageCount) {
        mLabelPage  = mPages.page;
        mLabelCount = pageCount;
        mPageLabel  = std::to_string(mPages.page + 1) + "/" + std::to_string(pageCount);
    }
    p.text(
        mPageLabel,
        mBounds.x + mBounds.w / 2,
        mPrev.rect.y + (mPrev.rect.h - p.lineHeight()) / 2 + 1,
        palette::TEXT_LIGHT,
        1,
        TextAlign::Center
    );

    if (mPages.itemCount == 0) {
        if (!emptyText.empty()) {
            p.text(emptyText, mGrid.bounds.x, mGrid.bounds.y + 4, palette::TEXT_DIM, 1, TextAlign::Left, mGrid.bounds.w);
        }
        return;
    }

    // Cells.
    auto& catalog = game::ItemCatalog::get();
    int   hovered = mGrid.hitTest(frame.mouseX, frame.mouseY);
    for (int i = 0, n = mGrid.capacity(); i < n; ++i) {
        auto const* key = cellKey(i);
        if (!key) break;
        auto cell = mGrid.cellRect(i);
        if (auto const* stack = catalog.displayStack(*key)) {
            p.itemStack(*stack, cell.x + 1, cell.y + 1);
        } else {
            p.fillRect({cell.x + 1, cell.y + 1, 16, 16}, palette::MISSING); // unknown item (e.g. stale bookmark)
        }
        if (i == hovered) p.fillRect({cell.x + 1, cell.y + 1, 16, 16}, palette::HOVER, Painter::Layer::Foreground);
    }
}

bool PagedItemGrid::onMouseButton(MouseButton button, bool down, float x, float y) {
    if (!contains(x, y)) return false;
    if (mGrid.bounds.contains(x, y) && ItemListPanel::cheatClickHook && cheatActive()
        && ItemListPanel::cheatClickHook(button, down, x, y)) {
        return true; // taken over by the hook
    }
    if (button == MouseButton::Middle && mGrid.bounds.contains(x, y)) return false; // input unit's hotkey
    if (!down) return true;
    if (button == MouseButton::Left) {
        if (mPrev.hit(x, y)) {
            mPages.step(-1);
            return true;
        }
        if (mNext.hit(x, y)) {
            mPages.step(1);
            return true;
        }
    }
    if (auto const* key = cellKey(mGrid.hitTest(x, y))) {
        core::ItemKey item = *key; // the action may change the list (e.g. bookmarks)
        activateItem(item, button);
    }
    return true;
}

bool PagedItemGrid::onScroll(float x, float y, float delta) {
    if (!contains(x, y)) return false;
    if (delta != 0) mPages.step(delta > 0 ? -1 : 1);
    return true;
}

bool PagedItemGrid::contains(float x, float y) const noexcept { return mShown && mBounds.contains(x, y); }

std::optional<core::ItemKey> PagedItemGrid::itemAt(float x, float y) const {
    if (!mShown) return std::nullopt;
    if (auto const* key = cellKey(mGrid.hitTest(x, y))) return *key;
    return std::nullopt;
}

} // namespace bei::ui
