#pragma once

namespace bei::input {

/// Bridges game input to BEI.
///
/// Contract:
/// - Mouse/keyboard events arrive on the window thread. The router decides synchronously
///   whether to swallow an event (using the last rendered frame's hit tests), and runs the
///   actual handling on the client thread (ll::thread::ClientThreadExecutor).
/// - Mouse: updates ui::UiRoot::setMouse (GUI units), routes buttons/wheel via UiRoot::dispatch*.
/// - Keys: while UiRoot::wantsTextInput(), all keys go to UiRoot::dispatchKey and are swallowed;
///   text goes to UiRoot::dispatchChar. Otherwise BEI hotkeys (remappable via LeviLamina's
///   KeyRegistry): R recipes, U uses, A bookmark, O toggle overlay, Backspace back, act on
///   app::Controller::hoveredItem().
/// - Tracks the hovered vanilla container slot and reports it via
///   app::Controller::setVanillaHovered.
/// - Resets transient state (held buttons, focus) on focus loss and level exit.
class InputRouter {
public:
    static InputRouter& get();

    void install();
    void uninstall();
};

} // namespace bei::input
