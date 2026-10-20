#pragma once

namespace bei::input {

/// Bridges game input to BEI.
///
/// Contract:
/// - Mouse/keyboard events arrive on the window thread. The router decides synchronously
///   whether to swallow an event (using the last rendered frame's hit tests), and runs the
///   actual handling on the client thread (once per frame, from ClientInstance::update).
/// - Mouse: updates ui::UiRoot::setMouse (GUI units), routes buttons/wheel via UiRoot::dispatch*.
/// - Keys: while UiRoot::wantsTextInput(), all keys go to UiRoot::dispatchKey and are swallowed;
///   text goes to UiRoot::dispatchChar. Otherwise BEI hotkeys (remappable via LeviLamina's
///   KeyRegistry, i.e. Settings > Keyboard & Mouse; detection matches raw key events against
///   the currently mapped keys): R recipes, U uses, A bookmark, O toggle overlay, Backspace back, act on
///   app::Controller::hoveredItem(). Hotkeys only fire on screens with a cursor (never in
///   gameplay) and never while a BEI or vanilla text box has focus. Backspace only acts while
///   the recipe screen is open.
/// - Mouse buttons/wheel are swallowed only when the cursor is over a BEI layer; a release is
///   swallowed iff its press was.
/// - Tracks the hovered vanilla container slot and reports it via
///   app::Controller::setVanillaHovered.
/// - Resets transient state (held buttons, focus) on focus loss and level exit.
class InputRouter {
public:
    static InputRouter& get();

    /// Register the remappable hotkeys with LeviLamina's KeyRegistry. Must be called from
    /// BEI::load: the game reads registered keys into its keyboard layout before mods are
    /// enabled. Idempotent; install() calls it too as a fallback (keys registered that late
    /// keep their defaults and are missing from the controls menu until the next start).
    void registerKeys();

    void install();
    void uninstall();
};

} // namespace bei::input
