#pragma once

#include "core/state/Layout.h"

#include <cstdint>

namespace bei::ui {

class TextBox;

/// Game-side plumbing for BEI text boxes:
/// - hooks the game's text-character path (typed letters, pasted/IME-composed text) and,
///   while a BEI layer wants text input, forwards it to UiRoot::dispatchChar instead of the game;
/// - claims the game keyboard (KeyboardManager) for the focused TextBox so typing does not
///   move the player, trigger vanilla shortcuts, and so the platform delivers text/IME input;
/// - releases it again when the box loses focus or stops being drawn (screen closed, level left).
///
/// Everything except install()/uninstall() runs on the client thread.
class TextInput {
public:
    static void install();
    static void uninstall();

    /// Called by TextBox::setFocused(true). Unfocuses any other focused box first.
    static void acquire(TextBox& box);
    /// Called by TextBox::setFocused(false). No-op if `box` is not the owner.
    static void release(TextBox& box);
    /// Called by ~TextBox: drops the reference without touching the game (which may already be
    /// gone during static destruction).
    static void forget(TextBox& box) noexcept;
    /// The focused box, or nullptr.
    [[nodiscard]] static TextBox* focusedBox() noexcept;

    /// Monotonic seconds. TextBox::render records it so a box that is no longer drawn loses
    /// focus automatically; also the clock for double clicks and key echo detection.
    [[nodiscard]] static double now() noexcept;
};

} // namespace bei::ui
