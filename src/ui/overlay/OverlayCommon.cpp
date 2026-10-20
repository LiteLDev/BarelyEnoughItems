// Pieces shared by the overlay panels: visibility rule and the hover tooltip layer.
#include "app/Controller.h"
#include "app/I18n.h"
#include "ui/UiRoot.h"
#include "ui/gfx/Painter.h"
#include "ui/overlay/PagedItemGrid.h"
#include "ui/recipe/RecipeScreen.h"
#include "ui/widgets/Tooltip.h"

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
} // namespace

void requestItemTooltip(core::ItemKey const& item) { gPending = item; }

BEI_REGISTER_UI_LAYER(gTooltipLayer);

} // namespace bei::ui
