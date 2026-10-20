// Recipe screen host: owns the native scene that gives BEI focus and a cursor, renders the
// recipe layer in that scene's place, and keeps isOpen() truthful. See RecipeScreen.h.
#include "ui/recipe/RecipeScreen.h"

#include "app/Controller.h"
#include "game/Game.h"
#include "mod/Log.h"
#include "ui/UiRoot.h"
#include "ui/recipe/RecipeLayer.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/thread/ClientThreadExecutor.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/AbstractScene.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <memory>

namespace bei::ui {

namespace {
using Clock = std::chrono::steady_clock;

/// How long a freshly pushed scene may take to reach the top of the stack before the
/// watchdog gives up on it (the game can drop a push during a screen transition).
constexpr auto WATCHDOG = std::chrono::seconds(3);

struct SceneState {
    std::shared_ptr<AbstractScene> scene;   ///< Our dialog scene while open (or closing).
    std::shared_ptr<AbstractScene> retired; ///< Popped scene, released on the next frame.
    Clock::time_point              openedAt;
    bool                           seen         = false; ///< Rendered at least once.
    bool                           closing      = false; ///< close() requested.
    bool                           popScheduled = false;
};

SceneState        gScene;          // client thread only
std::atomic<bool> gOwnsKeys{false}; // read on the window thread: our scene is top and open
bool              gReopen = false;  // show() while the pop of a closing scene is scheduled

ll::event::ListenerPtr gAfterRender;
ll::event::ListenerPtr gKey;
ll::event::ListenerPtr gExitLevel;

recipe::RecipeLayer& layer() { return recipe::RecipeLayer::get(); }

ScreenView* ownView() {
    if (!gScene.scene) return nullptr;
    // Bedrock scenes carry no RTTI; the dialog factory always creates a UIScene.
    auto* scene = static_cast<UIScene*>(gScene.scene.get());
    std::unique_ptr<ScreenView> const& view = scene->mScreenView;
    return view.get();
}

bool owns(UIScene const* scene) { return gScene.scene && gScene.scene.get() == scene; }

ISceneStack* sceneStack() {
    auto* client = game::currentClient();
    if (!client) return nullptr;
    return client->getSceneFactory().getCurrentSceneStack().get();
}

/// True if `target` is anywhere in the stack. The meaning of the callback's return value
/// (continue vs. stop) is not documented, so detect it from the number of visited scenes.
bool inStack(ISceneStack& stack, AbstractScene const* target) {
    bool        found   = false;
    std::size_t visited = 0;
    stack.forEachScreenConst(
        [&](AbstractScene const& s) {
            ++visited;
            if (&s == target) found = true;
            return !found;
        },
        true
    );
    if (found || visited >= stack.getSize()) return found;
    stack.forEachScreenConst(
        [&](AbstractScene const& s) {
            if (&s == target) found = true;
            return found;
        },
        true
    );
    return found;
}

/// Forget the scene. `byGame`: it went away without close() (ESC handled natively, a
/// dimension change, level exit...) so the navigation history is stale too.
void drop(bool byGame) {
    if (gScene.scene) gScene.retired = std::move(gScene.scene);
    gScene.scene.reset();
    gScene.seen         = false;
    gScene.closing      = false;
    gScene.popScheduled = false;
    gOwnsKeys           = false;
    log().info("Recipe screen closed{}", byGame ? " by the game" : "");
    if (gReopen && !byGame) return; // show() came in while closing: watch() pushes a new scene
    gReopen = false;
    layer().reset();
    if (byGame) app::Controller::get().history().clear();
}

/// Push the dialog scene that hosts the recipe screen (the layer already has its focus).
void pushScene() {
    auto* client = game::currentClient();
    auto  scene  = client ? client->getSceneFactory().createCommonDialogInfoScreen("BEI", "") : nullptr;
    if (!scene) {
        log().error("Recipe screen: could not create the dialog scene");
        layer().reset();
        return;
    }
    auto retired    = std::move(gScene.retired);
    gScene          = {};
    gScene.retired  = std::move(retired);
    gScene.scene    = std::move(scene);
    gScene.openedAt = Clock::now();
    client->getSceneFactory().getCurrentSceneStack()->pushScreen(gScene.scene, false);
}

/// Per-frame bookkeeping: key ownership, deferred pops/reopens and the watchdog.
void watch() {
    gScene.retired.reset();
    if (!gScene.scene && gReopen) {
        gReopen = false;
        pushScene();
    }
    if (!gScene.scene) {
        gOwnsKeys = false;
        return;
    }
    auto* stack = sceneStack();
    if (!stack) return;
    bool top  = stack->getTopScene() == gScene.scene.get();
    gOwnsKeys = top && !gScene.closing;
    if (top) {
        gScene.seen = true;
        if (gScene.closing && !gScene.popScheduled) {
            stack->schedulePopScreen(1);
            gScene.popScheduled = true;
        }
        return;
    }
    // Covered by another screen (e.g. the pause menu) is fine; gone from the stack is not.
    if (!inStack(*stack, gScene.scene.get()) && (gScene.seen || Clock::now() - gScene.openedAt > WATCHDOG)) {
        drop(!gScene.closing);
    }
}

// Our scene's view is drawn by BEI only. Highest priority makes this the outermost hook, so
// LeviLamina's Before/AfterUIRenderEvent never fire for it and UiRoot renders exactly once.
LL_TYPE_INSTANCE_HOOK(
    RecipeViewRenderHook,
    ll::memory::HookPriority::Highest,
    ScreenView,
    &ScreenView::render,
    void,
    ::UIRenderContext& uiRenderContext
) {
    if (!gScene.scene || this != ownView()) return origin(uiRenderContext);
    layer().setRendering(!gScene.closing);
    try {
        watch(); // no AfterUIRenderEvent fires for this view, so keep the bookkeeping going here
        UiRoot::get().renderFrame(*this, reinterpret_cast<MinecraftUIRenderContext&>(uiRenderContext));
    } catch (std::exception const& e) {
        log().error("Recipe screen render failed: {}", e.what());
    }
    layer().setRendering(false);
}

LL_TYPE_INSTANCE_HOOK(
    RecipeSceneExitHook,
    ll::memory::HookPriority::Normal,
    UIScene,
    &UIScene::$onScreenExit,
    void,
    bool                           isPopping,
    bool                           doScreenTransitions,
    std::shared_ptr<AbstractScene> pushedScene
) {
    bool mine = owns(this);
    // The dialog's exit animation is never shown; skip it.
    origin(isPopping, mine ? false : doScreenTransitions, std::move(pushedScene));
    if (mine && isPopping && owns(this)) drop(!gScene.closing);
}

LL_TYPE_INSTANCE_HOOK(
    RecipeSceneEntranceHook,
    ll::memory::HookPriority::Normal,
    UIScene,
    &UIScene::$onScreenEntrance,
    void,
    bool isRevisiting,
    bool doScreenTransitions
) {
    origin(isRevisiting, owns(this) ? false : doScreenTransitions);
}

// Keep the world visible behind the (dimmed) recipe screen.
LL_TYPE_INSTANCE_HOOK(RecipeSceneBehindHook, ll::memory::HookPriority::Normal, UIScene, &UIScene::$renderGameBehind, bool) {
    if (owns(this)) return true;
    return origin();
}

// JEI does not close when the player takes damage; neither do we.
LL_TYPE_INSTANCE_HOOK(RecipeSceneHurtHook, ll::memory::HookPriority::Normal, UIScene, &UIScene::$closeOnPlayerHurt, bool) {
    if (owns(this)) return false;
    return origin();
}

using Hooks = ll::memory::HookRegistrar<
    RecipeViewRenderHook,
    RecipeSceneExitHook,
    RecipeSceneEntranceHook,
    RecipeSceneBehindHook,
    RecipeSceneHurtHook>;
std::unique_ptr<Hooks> gHooks;
} // namespace

RecipeScreen& RecipeScreen::get() {
    static RecipeScreen instance;
    return instance;
}

void RecipeScreen::install() {
    gHooks   = std::make_unique<Hooks>();
    auto& bus = ll::event::EventBus::getInstance();
    gAfterRender = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto&) {
        try {
            watch();
        } catch (std::exception const& e) {
            log().error("Recipe screen watchdog failed: {}", e.what());
        }
    });
    gExitLevel = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
        gReopen = false;
        if (gScene.scene) drop(true); // the game flushes its scene stack itself
    });
    // Window thread: decide synchronously, act on the client thread.
    gKey = bus.emplaceListener<ll::event::KeyInputEvent>(
        [](ll::event::KeyInputEvent& ev) {
            if (!gOwnsKeys.load() || ev.isCancelled()) return;
            int vk = ev.keyCode();
            if (!recipe::RecipeLayer::isScreenKey(vk)) return;
            ev.cancel();
            if (!ev.isDown()) return;
            ll::thread::ClientThreadExecutor::getDefault().execute([vk] {
                if (RecipeScreen::get().isOpen()) layer().handleKey(vk);
            });
        },
        ll::event::EventPriority::High
    );
}

void RecipeScreen::uninstall() {
    close();
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* l : {&gAfterRender, &gKey, &gExitLevel}) {
        if (*l) bus.removeListener(*l);
        l->reset();
    }
    gHooks.reset();
    if (gScene.scene) drop(false);
    gScene.retired.reset();
}

void RecipeScreen::show(core::RecipeFocus const& focus) {
    layer().open(focus);
    log().info(
        "Recipe screen opened: {} ({})",
        focus.item.empty() ? focus.category : focus.item.toString(),
        focus.mode == core::LookupMode::Recipes ? "recipes" : "uses"
    );
    if (gScene.scene && !gScene.closing) return; // re-target the open screen
    if (gScene.scene && !gScene.popScheduled) {
        gScene.closing = false; // close() not carried out yet: keep the scene
        return;
    }
    if (gScene.scene) {
        // A pop is already scheduled and would hit a scene pushed now: reopen once it is gone.
        gReopen = true;
        return;
    }
    pushScene();
}

void RecipeScreen::close() {
    gReopen = false;
    if (!gScene.scene || gScene.closing) return;
    gScene.closing = true;
    gOwnsKeys      = false;
    auto* stack    = sceneStack();
    if (stack && stack->getTopScene() == gScene.scene.get()) {
        stack->schedulePopScreen(1);
        gScene.popScheduled = true;
    }
    // Otherwise watch() pops it once it is on top again (or notices it is gone).
}

bool RecipeScreen::isOpen() const { return (gScene.scene && !gScene.closing) || gReopen; }

std::optional<core::ItemKey> RecipeScreen::itemAt(float x, float y) const {
    if (!isOpen()) return std::nullopt;
    return layer().itemAt(x, y);
}

} // namespace bei::ui
