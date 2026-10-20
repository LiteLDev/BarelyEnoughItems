#pragma once

// Internal to src/input: state shared between the window thread (raw input events) and the
// client thread (game update / rendering / input handlers).

#include <atomic>
#include <functional>

namespace bei::input::detail {

/// Snapshot of client-thread state that window-thread callbacks use to decide synchronously
/// whether to swallow an event. Written on the client thread, read anywhere.
struct Snapshot {
    std::atomic<bool> inLevel{false};
    /// A screen with a free cursor is on top (not the HUD during gameplay).
    std::atomic<bool> cursorScreen{false};
    /// The last known cursor position is over a BEI layer.
    std::atomic<bool> overBei{false};
    /// A BEI text box has keyboard focus.
    std::atomic<bool> beiTextFocus{false};
    /// A vanilla text box had focus at the last key event (window thread writes this one).
    std::atomic<bool>  vanillaTextFocus{false};
    std::atomic<float> invGuiScale{1.f};
};

[[nodiscard]] Snapshot& snapshot();

/// Queue `fn` to run on the client thread at the next ClientInstance::update (once per frame,
/// outside rendering, also when no level ticks). Safe to call from any thread.
void post(std::function<void()> fn);

/// Install/uninstall the vanilla container slot hover tracking (SlotHover.cpp).
void installSlotHover();
void uninstallSlotHover();
/// Forget the hovered vanilla slot (client thread).
void clearSlotHover();

/// Create the KeyRegistry keys (once per process; must run during BEI::load).
void registerHotkeys();
/// Install/uninstall BEI hotkeys (Hotkeys.cpp).
void installHotkeys();
void uninstallHotkeys();
/// Window thread: a key was pressed (not a repeat) and no BEI text box took it.
void onHotkeyKeyDown(int vk);

} // namespace bei::input::detail
