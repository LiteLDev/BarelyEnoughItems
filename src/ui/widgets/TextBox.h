#pragma once

#include "core/state/Layout.h"

#include <cstddef>
#include <functional>
#include <string>

namespace bei::ui {

class Painter;

/// Single-line text input (used for the item-panel search bar).
///
/// Contract: the owner calls render/onMouseButton/onKey/onChar from its IUiLayer methods and
/// reports `focused()` from IUiLayer::wantsTextInput(). While focused the box must own the
/// game keyboard so typed letters do not move the player or trigger vanilla shortcuts.
/// Text is UTF-8; the caret moves by code point. `onChange` fires after every edit.
class TextBox {
public:
    core::Rect                              rect;
    std::string                             placeholder;
    std::function<void(std::string const&)> onChange;

    void render(Painter& painter, double time) const;
    /// Click inside focuses (right click clears, like JEI); click outside unfocuses.
    /// Returns true if the event was consumed.
    bool onMouseButton(bool right, bool down, float x, float y);
    bool onKey(int vk, bool down);
    bool onChar(std::string const& utf8);

    void setText(std::string text, bool notify = false);
    void setFocused(bool focused);

    [[nodiscard]] std::string const& text() const noexcept { return mText; }
    [[nodiscard]] bool               focused() const noexcept { return mFocused; }

private:
    std::string mText;
    std::size_t mCaret   = 0; ///< Byte offset (always on a code point boundary).
    bool        mFocused = false;
};

} // namespace bei::ui
