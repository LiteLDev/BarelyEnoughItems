// Pieces shared by the overlay panels: visibility rule and the hover tooltip layer.
#include "actions/Actions.h"
#include "app/Controller.h"
#include "app/I18n.h"
#include "ui/UiRoot.h"
#include "ui/gfx/Painter.h"
#include "ui/overlay/PagedItemGrid.h"
#include "ui/recipe/RecipeScreen.h"
#include "ui/widgets/Tooltip.h"

#include "mc/world/item/ItemStack.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace bei::ui {

bool overlayAllowed(FrameInfo const& frame) {
    if (!frame.inLevel || !frame.isContainerScreen) return false;
    auto const& cfg = app::Controller::get().config();
    if (!cfg.overlayEnabled) return false;
    if (!cfg.showOnAllContainers && !frame.screenName.starts_with("inventory_screen")
        && !frame.screenName.starts_with("crafting_screen")) {
        return false;
    }
    return !RecipeScreen::get().isOpen();
}

core::OverlayLayout overlayLayout(FrameInfo const& frame) {
    core::Rect used = frame.container;
    if (!used.empty()) {
        float left  = std::min(used.x, frame.screen.w - used.right());
        float right = std::max(used.right(), frame.screen.w - used.x);
        left        = std::max(left, 0.f);
        right       = std::min(right, frame.screen.w);
        used.x      = left;
        used.w      = right - left;
    }
    core::OverlayLayoutOptions options;
    options.cell                    = PagedItemGrid::CELL;
    options.pageControlsHeight      = PagedItemGrid::HEADER_H;
    options.searchBarHeight         = 16;
    options.centerSearchBar         = app::Controller::get().config().centerSearchBar;
    options.centerSearchBarMaxWidth = frame.container.w;
    return core::computeOverlayLayout(frame.screen.w, frame.screen.h, used, options);
}

bool reopened(FrameInfo const& frame, double& lastTime) {
    bool result = lastTime < 0 || frame.time - lastTime > 0.25 || frame.time < lastTime;
    lastTime    = frame.time;
    return result;
}

namespace {
std::optional<core::ItemKey> gPending;

/// Draws the tooltip of the hovered panel item above every overlay panel.
class OverlayTooltipLayer final : public IUiLayer {
public:
    [[nodiscard]] char const* name() const override { return "overlay_tooltip"; }
    [[nodiscard]] int         zOrder() const override { return 150; }
    [[nodiscard]] bool        visible(FrameInfo const& frame) const override { return overlayAllowed(frame); }

    void render(Painter& p, FrameInfo const& frame) override {
        if (reopened(frame, mLastTime)) mCheatHint = app::tr("bei.overlay.cheat_hint", "Click: give stack   Shift+Click: give one");
        if (!gPending) return;
        auto item = std::move(*gPending);
        gPending.reset();
        if (cheatActive()) {
            if (mExtra.empty()) mExtra.push_back({});
            mExtra.front() = {mCheatHint, 0xA0A0A0};
            drawItemTooltip(p, item, frame.mouseX, frame.mouseY, frame, mExtra);
        } else {
            drawItemTooltip(p, item, frame.mouseX, frame.mouseY, frame);
        }
    }

private:
    double                   mLastTime = -1;
    std::string              mCheatHint;
    std::vector<TooltipLine> mExtra;
};
OverlayTooltipLayer gTooltipLayer;

/// The stack held on the vanilla cursor, drawn again above the overlay panels (and above the
/// vanilla tooltips BEI defers past them), where vanilla draws it with the screen. It covers the
/// vanilla icon exactly: same size, centered on the cursor.
class CursorStackLayer final : public IUiLayer {
public:
    [[nodiscard]] char const* name() const override { return "cursor_stack"; }
    [[nodiscard]] int         zOrder() const override { return UiRoot::VANILLA_OVERLAY_Z + 100; }
    [[nodiscard]] bool        visible(FrameInfo const& frame) const override {
        return overlayAllowed(frame) && frame.mouseX >= 0 && actions::Actions::get().hasCursorItem();
    }

    void render(Painter& p, FrameInfo const& frame) override {
        auto const* stack = actions::Actions::get().cursorItem();
        if (!stack) return;
        float x = frame.mouseX - 8, y = frame.mouseY - 8;
        p.itemStack(*stack, x, y);
        p.count(stack->mCount, x, y);
    }
};
CursorStackLayer gCursorStackLayer;
} // namespace

void requestItemTooltip(core::ItemKey const& item) { gPending = item; }

BEI_REGISTER_UI_LAYER(gTooltipLayer);
BEI_REGISTER_UI_LAYER(gCursorStackLayer);

} // namespace bei::ui
