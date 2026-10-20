#include "ui/UiRoot.h"

#include "app/Controller.h"
#include "game/Game.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/controls/renderers/HoverTextRenderer.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <vector>

namespace bei::ui {

namespace {
ll::event::ListenerPtr gAfterRender;
auto const             gStart = std::chrono::steady_clock::now();

/// Vanilla control names of the main container panel, most specific first.
constexpr std::array CONTAINER_CONTROLS{"root_panel", "inventory_panel_bottom_half_with_label", "common_panel"};
/// The inventory and crafting table screens (crafting.recipe_inventory_screen_content) have no
/// root_panel; their common_panel belongs to the player inventory, which the creative layout hides
/// (keeping a stale, narrower rect). In that layout the window is the creative item list
/// ("recipe_book") plus the creative hotbar below it.
constexpr std::array CREATIVE_CONTROLS{"recipe_book", "creative_hotbar_panel"};

core::Rect controlRect(UIControl& control) {
    control._setCachedPosition();
    glm::vec2 pos  = control.mCachedPosition;
    glm::vec2 size = control.mSize;
    if (!(size.x > 0 && size.y > 0)) return {};
    return {pos.x, pos.y, size.x, size.y};
}

core::Rect unite(core::Rect const& a, core::Rect const& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    float l = std::min(a.x, b.x), t = std::min(a.y, b.y);
    return {l, t, std::max(a.right(), b.right()) - l, std::max(a.bottom(), b.bottom()) - t};
}

core::Rect findContainerRect(ScreenView& view, std::string const& screenName) {
    auto& tree = view.mVisualTree;
    if (!tree) return {};
    if (screenName.starts_with("inventory_screen") || screenName.starts_with("crafting_screen")) {
        auto inventory = tree->getControlByName("player_inventory", true);
        if (inventory && !inventory->mIsVisibleInTree) {
            core::Rect rect;
            for (auto const* name : CREATIVE_CONTROLS) {
                auto control = tree->getControlByName(name, true);
                if (control && control->mIsVisibleInTree) rect = unite(rect, controlRect(*control));
            }
            if (!rect.empty()) return rect;
        }
    }
    for (auto const* name : CONTAINER_CONTROLS) {
        auto control = tree->getControlByName(name, true);
        if (!control) continue;
        if (auto rect = controlRect(*control); !rect.empty()) return rect;
    }
    return {};
}

// --- Vanilla tooltips above BEI ---------------------------------------------------------------
// BEI draws after the whole vanilla UI pass, so vanilla hover text (e.g. the crafting output's
// tooltip) would end up under the overlay panels. While BEI shows layers, the hover text renderer
// is skipped during the vanilla pass and replayed after BEI's layers below
// UiRoot::VANILLA_OVERLAY_Z, in the same frame (all pointers stay valid until then).

bool gDeferTooltips = false; ///< BEI rendered layers in the last frame.

struct DeferredTooltip {
    HoverTextRenderer*        renderer;
    MinecraftUIRenderContext* ctx;
    IClientInstance*          client;
    UIControl*                owner;
    int                       pass;
};
std::vector<DeferredTooltip> gDeferred;

LL_TYPE_INSTANCE_HOOK(
    DeferTooltipHook,
    ll::memory::HookPriority::Normal,
    HoverTextRenderer,
    &HoverTextRenderer::$render,
    void,
    ::MinecraftUIRenderContext& ctx,
    ::IClientInstance&          client,
    ::UIControl&                owner,
    int                         pass
) {
    if (!gDeferTooltips) return origin(ctx, client, owner, pass);
    gDeferred.push_back({this, &ctx, &client, &owner, pass});
}

void replayTooltips() {
    if (gDeferred.empty()) return;
    auto pending = std::move(gDeferred);
    gDeferred.clear();
    for (auto const& t : pending) {
        static_cast<DeferTooltipHook*>(t.renderer)->origin(*t.ctx, *t.client, *t.owner, t.pass);
    }
}
} // namespace

UiRoot& UiRoot::get() {
    static UiRoot instance;
    return instance;
}

void UiRoot::add(IUiLayer* layer) { mLayers.push_back(layer); }

void UiRoot::install() {
    DeferTooltipHook::hook();
    gAfterRender = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([this](auto& ev) {
        auto& view = ev.screenView();
        // Several screen views render with focus in one frame (e.g. "toast_screen" after
        // "inventory_screen"). Only the top-most scene hosts BEI; otherwise the later pass
        // replaces the hit-test state of the screen the player is looking at, and hover
        // hotkeys (R/U/A) find nothing.
        bool host = view.mHasFocus;
        if (auto* client = game::currentClient(); host && client) {
            auto top   = client->getTopScreenName();
            auto scene = ev.uiRenderContext().mCurrentScene.getScreenName();
            host       = top.empty() || scene == top;
        }
        if (host) {
            try {
                renderFrame(view, ev.uiRenderContext());
            } catch (std::exception const& e) {
                log().error("UI render failed: {}", e.what());
            }
        }
        replayTooltips(); // whatever this view deferred and renderFrame did not draw
    });
}

void UiRoot::uninstall() {
    if (gAfterRender) ll::event::EventBus::getInstance().removeListener(gAfterRender);
    gAfterRender.reset();
    DeferTooltipHook::unhook();
    gDeferTooltips = false;
    gDeferred.clear();
    mVisible.clear();
}

void UiRoot::renderFrame(ScreenView& view, MinecraftUIRenderContext& ctx) {
    app::Controller::get().tick(wantsTextInput()); // previous frame's layers
    Painter painter(ctx);
    auto*   client       = game::currentClient();
    mFrame.screen        = painter.screen();
    mFrame.screenName    = client ? client->getScreenName() : std::string{};
    mFrame.inLevel       = game::currentLevel() != nullptr;
    auto& controller     = view.mController;
    mFrame.isContainerScreen = controller && controller->_isContainerScreen();
    auto container       = mFrame.isContainerScreen ? findContainerRect(view, mFrame.screenName) : core::Rect{};
    if (container != mFrame.container) {
        log().info("Container rect on {}: {},{} {}x{}", mFrame.screenName, container.x, container.y, container.w, container.h);
    }
    mFrame.container = container;
    mFrame.time = std::chrono::duration<double>(std::chrono::steady_clock::now() - gStart).count();

    std::vector<IUiLayer*> visible;
    for (auto* layer : mLayers) {
        if (layer->visible(mFrame)) visible.push_back(layer);
    }
    std::stable_sort(visible.begin(), visible.end(), [](auto* a, auto* b) { return a->zOrder() < b->zOrder(); });
    bool replayed = false;
    for (auto* layer : visible) {
        if (!replayed && layer->zOrder() >= VANILLA_OVERLAY_Z) {
            replayTooltips();
            replayed = true;
        }
        layer->render(painter, mFrame);
        painter.flush();
    }
    if (!replayed) replayTooltips();
    std::reverse(visible.begin(), visible.end());
    gDeferTooltips = !visible.empty();
    mVisible       = std::move(visible);
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
