// Input routing: raw mouse/keyboard events (window thread) -> BEI layers (client thread),
// hotkeys (Hotkeys.cpp) and vanilla slot hover (SlotHover.cpp).
//
// Threading model
// - MouseInputEvent/KeyInputEvent arrive on the window thread. They only read the atomic
//   Snapshot (refreshed on the client thread every frame) to decide whether to cancel, and
//   post the real work to the client thread. They never touch UiRoot or game state directly.
// - Posted work runs from a ClientInstance::update hook: once per frame, outside rendering,
//   and also when no level ticks (ll's ClientThreadExecutor runs on ClientLevelTickEvent,
//   i.e. 20 Hz and only while a level ticks, which is too coarse for cursor tracking and
//   would stall while a pausing screen is open).
#include "input/InputRouter.h"

#include "input/InputState.h"

#include "game/Game.h"
#include "mod/Log.h"
#include "ui/UiRoot.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/gui/GuiData.h"
#include "mc/deps/input/HIDController.h"
#include "mc/deps/input/MouseAction.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <mutex>
#include <vector>

namespace bei::input {

namespace detail {

namespace {

// --- Client-thread work queue ----------------------------------------------------------------

constexpr std::size_t MAX_QUEUED = 1024; // drop input rather than grow without bound

std::mutex                         gQueueMutex;
std::vector<std::function<void()>> gQueue;

/// Latest absolute cursor position (GUI units), coalesced: only the newest one matters.
constexpr std::uint64_t    NO_MOUSE = ~std::uint64_t{0};
std::atomic<std::uint64_t> gPendingMouse{NO_MOUSE};
/// Latest absolute cursor position seen on the window thread (never consumed).
std::atomic<std::uint64_t> gLastMouse{NO_MOUSE};

std::uint64_t packMouse(float x, float y) {
    return (std::uint64_t{std::bit_cast<std::uint32_t>(x)} << 32) | std::bit_cast<std::uint32_t>(y);
}

// --- Window-thread state ---------------------------------------------------------------------

constexpr std::size_t BUTTONS = 3;
/// Mouse buttons whose press BEI swallowed; their release is swallowed too.
std::array<std::atomic<bool>, BUTTONS> gHeldButtons{};
/// Keys whose press went to a BEI text box; their release goes there too.
std::array<std::atomic<bool>, 256> gHeldKeys{};
/// Physical key state, to tell a new press from OS key repeat.
std::array<std::atomic<bool>, 256> gKeysDown{};
/// Time (steady ms) of each key's last down event; OS repeats arrive well within the window.
std::array<std::atomic<std::int64_t>, 256> gKeyDownTime{};
constexpr std::int64_t                     REPEAT_WINDOW_MS = 1100;

ll::event::ListenerPtr gMouseListener;
ll::event::ListenerPtr gKeyListener;
ll::event::ListenerPtr gAfterRenderListener;
ll::event::ListenerPtr gExitLevelListener;

void runQueued() {
    std::vector<std::function<void()>> work;
    {
        std::lock_guard lock{gQueueMutex};
        work.swap(gQueue);
    }
    for (auto& fn : work) {
        try {
            fn();
        } catch (std::exception const& e) {
            log().error("Input handler failed: {}", e.what());
        } catch (...) {
            log().error("Input handler failed");
        }
    }
}

// --- Hit testing for the window thread ---------------------------------------------------------
// Exact answer at the cursor position the client last saw, plus a coarse map of the whole
// screen for events that arrive before the client caught up with a fast cursor move.

/// Cursor position (GUI units) that Snapshot::overBei was computed for.
std::atomic<std::uint64_t> gHitMouse{NO_MOUSE};

constexpr int GRID_CELLS = 64; // cells along the longer screen side
struct HitGrid {
    std::atomic<float>                                                   originX{0}, originY{0}, cell{0};
    std::atomic<int>                                                     cols{0}, rows{0};
    std::array<std::atomic<std::uint64_t>, GRID_CELLS * GRID_CELLS / 64> bits{};
};
HitGrid gGrid;

void refreshHitGrid(bool live) {
    auto& root   = ui::UiRoot::get();
    auto& screen = root.lastFrame().screen;
    if (!live || screen.empty()) {
        gGrid.cols = 0;
        return;
    }
    // Layers only change what they cover when they render: rebuild once per rendered frame.
    static double builtFor = -1;
    if (gGrid.cols.load() > 0 && root.lastFrame().time == builtFor) return;
    builtFor   = root.lastFrame().time;
    float cell = std::max(screen.w, screen.h) / GRID_CELLS;
    int   cols = std::min(GRID_CELLS, static_cast<int>(std::ceil(screen.w / cell)));
    int   rows = std::min(GRID_CELLS, static_cast<int>(std::ceil(screen.h / cell)));
    std::array<std::uint64_t, GRID_CELLS * GRID_CELLS / 64> bits{};
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = screen.x + (static_cast<float>(c) + 0.5f) * cell;
            float y = screen.y + (static_cast<float>(r) + 0.5f) * cell;
            if (root.contains(x, y)) {
                int i                                   = r * GRID_CELLS + c;
                bits[static_cast<std::size_t>(i / 64)] |= std::uint64_t{1} << (i % 64);
            }
        }
    }
    for (std::size_t i = 0; i < bits.size(); ++i) gGrid.bits[i] = bits[i];
    gGrid.originX = screen.x;
    gGrid.originY = screen.y;
    gGrid.cell    = cell;
    gGrid.rows    = rows;
    gGrid.cols    = cols;
}

/// Whether (x, y) is over a BEI layer, as far as the window thread can tell.
bool overBeiAt(float x, float y) {
    auto& snap = snapshot();
    if (auto packed = gHitMouse.load(); packed != NO_MOUSE) {
        float hx = std::bit_cast<float>(static_cast<std::uint32_t>(packed >> 32));
        float hy = std::bit_cast<float>(static_cast<std::uint32_t>(packed));
        if (std::abs(hx - x) <= 1.f && std::abs(hy - y) <= 1.f) return snap.overBei.load();
    }
    int   cols = gGrid.cols.load();
    float cell = gGrid.cell.load();
    if (cols <= 0 || cell <= 0) return snap.overBei.load();
    int c = static_cast<int>(std::floor((x - gGrid.originX.load()) / cell));
    int r = static_cast<int>(std::floor((y - gGrid.originY.load()) / cell));
    if (c < 0 || r < 0 || c >= cols || r >= gGrid.rows.load()) return false;
    int i = r * GRID_CELLS + c;
    return (gGrid.bits[static_cast<std::size_t>(i / 64)].load() >> (i % 64)) & 1u;
}

/// Hit tests for the window thread, from the last frame's layers at the last cursor position.
void refreshHitSnapshot() {
    auto& snap        = snapshot();
    auto& root        = ui::UiRoot::get();
    auto& frame       = root.lastFrame();
    bool  live        = snap.inLevel.load() && snap.cursorScreen.load();
    snap.overBei      = live && root.contains(frame.mouseX, frame.mouseY);
    snap.beiTextFocus = live && root.wantsTextInput();
    gHitMouse         = packMouse(frame.mouseX, frame.mouseY);
}

/// Forget held buttons/keys whose release may never arrive; tell BEI layers they were released.
void releaseHeld(bool notifyLayers) {
    for (std::size_t i = 0; i < BUTTONS; ++i) {
        if (!gHeldButtons[i].exchange(false) || !notifyLayers) continue;
        auto button = static_cast<ui::MouseButton>(i);
        post([button] {
            auto& frame = ui::UiRoot::get().lastFrame();
            ui::UiRoot::get().dispatchMouseButton(button, false, frame.mouseX, frame.mouseY);
        });
    }
    for (std::size_t vk = 0; vk < gHeldKeys.size(); ++vk) {
        if (!gHeldKeys[vk].exchange(false) || !notifyLayers) continue;
        post([vk] { ui::UiRoot::get().dispatchKey(static_cast<int>(vk), false); });
    }
    for (auto& key : gKeysDown) key = false;
}

void onClientUpdate(ClientInstance& client) {
    auto& snap        = snapshot();
    snap.invGuiScale  = client.getGuiData()->mInvGuiScale;
    bool inLevel      = client.getLevel() != nullptr;
    bool wasCursor    = snap.cursorScreen.load();
    bool cursor       = inLevel && !client.getMouseGrabbed() && client.getScreenName() != "hud_screen";
    snap.inLevel      = inLevel;
    snap.cursorScreen = cursor;
    // Back to gameplay: whatever slot was hovered is gone, even if a screen controller
    // skipped the unhover/leave callbacks.
    if (wasCursor && !cursor) clearSlotHover();

    if (auto packed = gPendingMouse.exchange(NO_MOUSE); packed != NO_MOUSE) {
        ui::UiRoot::get().setMouse(
            std::bit_cast<float>(static_cast<std::uint32_t>(packed >> 32)),
            std::bit_cast<float>(static_cast<std::uint32_t>(packed))
        );
    }
    runQueued();
    refreshHitSnapshot();
    refreshHitGrid(inLevel && cursor);
}

// --- Event handlers (window thread) ----------------------------------------------------------

void onMouse(ll::event::MouseInputEvent& ev) {
    auto& snap   = snapshot();
    float scale  = snap.invGuiScale.load();
    float x      = static_cast<float>(ev.x()) * scale;
    float y      = static_cast<float>(ev.y()) * scale;
    auto  action = static_cast<int>(ev.actionButtonId());

    switch (action) {
    case MouseAction::ActionMove:
        gLastMouse    = packMouse(x, y);
        gPendingMouse = packMouse(x, y);
        return;
    case MouseAction::ActionMoveRelative:
        return; // gameplay camera look
    case MouseAction::ActionWheel: {
        // Wheel events do not carry a reliable cursor position: use the last known one.
        if (auto last = gLastMouse.load(); last != NO_MOUSE) {
            x = std::bit_cast<float>(static_cast<std::uint32_t>(last >> 32));
            y = std::bit_cast<float>(static_cast<std::uint32_t>(last));
        }
        if (!snap.cursorScreen.load() || !overBeiAt(x, y)) return;
        float delta = static_cast<float>(ev.buttonData());
        post([x, y, delta] { ui::UiRoot::get().dispatchScroll(x, y, delta); });
        ev.cancel();
        return;
    }
    default:
        break;
    }

    ui::MouseButton button;
    switch (action) {
    case MouseAction::ActionLeft:
        button = ui::MouseButton::Left;
        break;
    case MouseAction::ActionRight:
        button = ui::MouseButton::Right;
        break;
    case MouseAction::ActionMiddle:
        button = ui::MouseButton::Middle;
        break;
    default:
        return;
    }
    auto& held = gHeldButtons[static_cast<std::size_t>(button)];
    bool  down = ev.buttonData() == MouseAction::DataDown;
    bool  live = snap.inLevel.load() && snap.cursorScreen.load();

    bool consume;
    if (down) {
        consume = live && overBeiAt(x, y);
        held    = consume; // also drops a stale flag from a release that never arrived
        if (!live) return;
    } else {
        consume = held.exchange(false);
        if (!consume && !live) return;
    }
    gLastMouse    = packMouse(x, y);
    gPendingMouse = packMouse(x, y);
    // Layers see every press/release on cursor screens (e.g. a click elsewhere unfocuses the
    // search box); vanilla only loses the ones that hit BEI.
    post([button, down, x, y] { ui::UiRoot::get().dispatchMouseButton(button, down, x, y); });
    if (consume) ev.cancel();
}

void onKey(ll::event::KeyInputEvent& ev) {
    auto& snap            = snapshot();
    snap.vanillaTextFocus = static_cast<bool>(ev.controller().mTextboxIsFocused);
    int  vk               = ev.keyCode();
    bool down             = ev.isDown();
    if (vk < 0 || vk >= static_cast<int>(gHeldKeys.size())) return;
    auto& held = gHeldKeys[static_cast<std::size_t>(vk)];
    // OS key repeat sends more downs while a key is held; hotkeys act once per press. A
    // listener with higher priority may cancel a release (LeviLamina then stops propagation,
    // so we never see it); the time window keeps such a stale "down" from eating the next press.
    auto idx = static_cast<std::size_t>(vk);
    auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    bool repeat = gKeysDown[idx].exchange(down) && down && now - gKeyDownTime[idx].load() < REPEAT_WINDOW_MS;
    if (down) gKeyDownTime[idx] = now;

    bool consume;
    if (down) {
        consume = snap.beiTextFocus.load();
        if (consume) held = true;
        // Keys another component consumed first (e.g. Backspace/ESC by the recipe screen) never
        // reach this Low-priority listener, so they cannot act twice.
        else if (!repeat) onHotkeyKeyDown(vk);
    } else {
        // A release follows its press: to the text box if it got the press, else to vanilla.
        consume = held.exchange(false);
    }
    if (!consume) return;
    post([vk, down] { ui::UiRoot::get().dispatchKey(vk, down); });
    ev.cancel();
}

} // namespace

Snapshot& snapshot() {
    static Snapshot instance;
    return instance;
}

void post(std::function<void()> fn) {
    std::lock_guard lock{gQueueMutex};
    if (gQueue.size() >= MAX_QUEUED) {
        static std::atomic<bool> warned{false};
        if (!warned.exchange(true)) log().warn("Input queue full: the client thread is not draining; dropping input");
        return;
    }
    gQueue.push_back(std::move(fn));
}

// --- Hooks -------------------------------------------------------------------------------------

LL_TYPE_INSTANCE_HOOK(
    InputClientUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool isInitFinished
) {
    auto result = origin(isInitFinished);
    if (!isInitFinished) return result; // GuiData and friends may not exist yet
    try {
        onClientUpdate(*this);
    } catch (std::exception const& e) {
        log().error("Input update failed: {}", e.what());
    }
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    InputFocusLostHook,
    ll::memory::HookPriority::Normal,
    MinecraftGame,
    &MinecraftGame::$onAppFocusLost,
    void
) {
    origin();
    // Releases may never arrive while unfocused: forget held buttons/keys and the hovered slot.
    releaseHeld(true);
    post([] { clearSlotHover(); });
}

} // namespace detail

InputRouter& InputRouter::get() {
    static InputRouter instance;
    return instance;
}

void InputRouter::registerKeys() { detail::registerHotkeys(); }

void InputRouter::install() {
    using namespace detail;
    auto& bus      = ll::event::EventBus::getInstance();
    gMouseListener = bus.emplaceListener<ll::event::MouseInputEvent>(onMouse);
    // Low priority: components that consume keys themselves (recipe screen, text input) run
    // first and cancel the event, which onKey respects for hotkeys.
    gKeyListener = bus.emplaceListener<ll::event::KeyInputEvent>(onKey, ll::event::EventPriority::Low);
    // Low priority: after UiRoot (Normal) rebuilt this frame's visible layers.
    gAfterRenderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>(
        [](ll::event::AfterUIRenderEvent&) { refreshHitSnapshot(); },
        ll::event::EventPriority::Low
    );
    gExitLevelListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](ll::event::ClientExitLevelEvent&) {
        auto& snap        = snapshot();
        snap.inLevel      = false;
        snap.cursorScreen = false;
        snap.overBei      = false;
        snap.beiTextFocus = false;
        releaseHeld(false);
        clearSlotHover();
    });
    InputClientUpdateHook::hook();
    InputFocusLostHook::hook();
    installSlotHover();
    installHotkeys();
}

void InputRouter::uninstall() {
    using namespace detail;
    uninstallHotkeys();
    uninstallSlotHover();
    InputFocusLostHook::unhook();
    InputClientUpdateHook::unhook();
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* l : {&gMouseListener, &gKeyListener, &gAfterRenderListener, &gExitLevelListener}) {
        if (*l) bus.removeListener(*l);
        l->reset();
    }
    releaseHeld(false);
    std::lock_guard lock{gQueueMutex};
    gQueue.clear();
}

} // namespace bei::input
