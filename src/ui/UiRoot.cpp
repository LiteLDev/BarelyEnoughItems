#include "ui/UiRoot.h"

#include "game/Game.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>

namespace bei::ui {

namespace {
ll::event::ListenerPtr gAfterRender;
auto const             gStart = std::chrono::steady_clock::now();

/// Vanilla control names of the main container panel, most specific first.
constexpr std::array CONTAINER_CONTROLS{"root_panel", "inventory_panel_bottom_half_with_label", "common_panel"};

core::Rect findContainerRect(ScreenView& view) {
    auto& tree = view.mVisualTree;
    if (!tree) return {};
    for (auto const* name : CONTAINER_CONTROLS) {
        auto control = tree->getControlByName(name, true);
        if (!control) continue;
        control->_setCachedPosition();
        glm::vec2 pos  = control->mCachedPosition;
        glm::vec2 size = control->mSize;
        if (size.x > 0 && size.y > 0) return {pos.x, pos.y, size.x, size.y};
    }
    return {};
}
} // namespace

UiRoot& UiRoot::get() {
    static UiRoot instance;
    return instance;
}

void UiRoot::add(IUiLayer* layer) { mLayers.push_back(layer); }

void UiRoot::install() {
    gAfterRender = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([this](auto& ev) {
        auto& view = ev.screenView();
        if (!view.mHasFocus) return;
        // Several screen views render with focus in one frame (e.g. "toast_screen" after
        // "inventory_screen"). Only the top-most scene hosts BEI; otherwise the later pass
        // replaces the hit-test state of the screen the player is looking at, and hover
        // hotkeys (R/U/A) find nothing.
        auto* client = game::currentClient();
        if (client) {
            auto top   = client->getTopScreenName();
            auto scene = ev.uiRenderContext().mCurrentScene.getScreenName();
            if (!top.empty() && scene != top) return;
        }
        try {
            renderFrame(view, ev.uiRenderContext());
        } catch (std::exception const& e) {
            log().error("UI render failed: {}", e.what());
        }
    });
}

void UiRoot::uninstall() {
    if (gAfterRender) ll::event::EventBus::getInstance().removeListener(gAfterRender);
    gAfterRender.reset();
    mVisible.clear();
}

void UiRoot::renderFrame(ScreenView& view, MinecraftUIRenderContext& ctx) {
    Painter painter(ctx);
    auto*   client       = game::currentClient();
    mFrame.screen        = painter.screen();
    mFrame.screenName    = client ? client->getScreenName() : std::string{};
    mFrame.inLevel       = game::currentLevel() != nullptr;
    auto& controller     = view.mController;
    mFrame.isContainerScreen = controller && controller->_isContainerScreen();
    mFrame.container     = mFrame.isContainerScreen ? findContainerRect(view) : core::Rect{};
    mFrame.time = std::chrono::duration<double>(std::chrono::steady_clock::now() - gStart).count();

    std::vector<IUiLayer*> visible;
    for (auto* layer : mLayers) {
        if (layer->visible(mFrame)) visible.push_back(layer);
    }
    std::stable_sort(visible.begin(), visible.end(), [](auto* a, auto* b) { return a->zOrder() < b->zOrder(); });
    for (auto* layer : visible) {
        layer->render(painter, mFrame);
        painter.flush();
    }
    std::reverse(visible.begin(), visible.end());
    mVisible = std::move(visible);
}

bool UiRoot::dispatchMouseButton(MouseButton button, bool down, float x, float y) {
    for (auto* l : mVisible) {
        if (l->onMouseButton(button, down, x, y)) return true;
    }
    return false;
}

bool UiRoot::dispatchScroll(float x, float y, float delta) {
    for (auto* l : mVisible) {
        if (l->onScroll(x, y, delta)) return true;
    }
    return false;
}

bool UiRoot::dispatchKey(int vk, bool down) {
    for (auto* l : mVisible) {
        if (l->onKey(vk, down)) return true;
    }
    return false;
}

bool UiRoot::dispatchChar(std::string const& utf8) {
    for (auto* l : mVisible) {
        if (l->wantsTextInput() && l->onChar(utf8)) return true;
    }
    return false;
}

bool UiRoot::wantsTextInput() const {
    return std::any_of(mVisible.begin(), mVisible.end(), [](auto* l) { return l->wantsTextInput(); });
}

bool UiRoot::contains(float x, float y) const {
    return std::any_of(mVisible.begin(), mVisible.end(), [&](auto* l) { return l->contains(x, y); });
}

std::optional<core::ItemKey> UiRoot::itemAt(float x, float y) const {
    for (auto* l : mVisible) {
        if (auto item = l->itemAt(x, y)) return item;
        if (l->contains(x, y)) return std::nullopt; // an opaque layer hides what is below
    }
    return std::nullopt;
}

} // namespace bei::ui
