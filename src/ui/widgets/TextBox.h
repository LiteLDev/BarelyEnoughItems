#pragma once

#include "core/state/HistoryCursor.h"
#include "core/state/Layout.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bei::ui {

class Painter;

/// Single-line text input (used for the item-panel search bar).
///
/// Contract: the owner calls render/onMouseButton/onKey/onChar from its IUiLayer methods and
/// reports `focused()` from IUiLayer::wantsTextInput(). While focused the box must own the
/// game keyboard so typed letters do not move the player or trigger vanilla shortcuts.
/// Text is UTF-8; the caret moves by code point. `onChange` fires after every edit.
///
/// Editing: Left/Right (Ctrl: by word, Shift: extend selection), Home/End, Backspace/Delete
/// (Ctrl: whole word), Ctrl+A/C/X/V (Windows clipboard), double click selects all, right click
/// clears, Enter/Esc unfocus, Up/Down browse `history`. Text wider than the box scrolls horizontally to keep the caret
/// visible. Focus claims the game keyboard (see TextInput); a box that is not drawn for a few
/// frames loses focus automatically. Call everything on the client thread.
class TextBox {
public:
    core::Rect                              rect;
    std::string                             placeholder;
    std::function<void(std::string const&)> onChange;
    std::size_t                             maxLength = 256; ///< In code points.
    std::uint32_t                           textColor = 0xE0E0E0;
    /// Fired when text is submitted: Enter, or losing focus (any way) with non-blank text.
    std::function<void(std::string const&)> onSubmit;
    /// Optional submitted-text history, newest first (e.g. Config::searchHistory). Up/Down while
    /// focused replace the text with older/newer entries (Down past the newest restores what was
    /// typed); any edit or focus change ends the navigation.
    std::function<std::vector<std::string> const&()> history;

    TextBox() = default;
    ~TextBox();
    /// Copies never inherit focus (only one box owns the game keyboard).
    TextBox(TextBox const& other);
    TextBox& operator=(TextBox const& other);

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

    /// Caret byte offset (always on a code point boundary).
    [[nodiscard]] std::size_t caret() const noexcept { return mCaret; }
    [[nodiscard]] bool        hasSelection() const noexcept { return mAnchor != mCaret; }
    [[nodiscard]] std::string selectedText() const;
    void                      selectAll();
    /// Replace the selection (or insert at the caret); control characters are dropped and the
    /// result is clipped to maxLength. Fires onChange if the text changed.
    void insert(std::string const& utf8);
    /// TextInput uses this to unfocus boxes that are no longer drawn.
    [[nodiscard]] double lastRenderTime() const noexcept { return mLastRenderTime; }

private:
    struct Glyph {
        std::size_t offset; ///< Byte offset of the boundary.
        float       x;      ///< Distance from the first visible character.
    };

    void                      setCaret(std::size_t pos, bool extend);
    void                      eraseRange(std::size_t from, std::size_t to);
    void                      deleteSelection();
    void                      changed();
    void                      submit();
    void                      browseHistory(bool older);
    void                      restartBlink() noexcept { mBlinkOrigin = mLastTime; }
    [[nodiscard]] std::size_t offsetAt(float x) const;
    [[nodiscard]] bool        ctrlDown() const;
    [[nodiscard]] bool        shiftDown() const;
    void                      layout(Painter& painter) const;

    std::string mText;
    std::size_t mCaret   = 0; ///< Byte offset (always on a code point boundary).
    std::size_t mAnchor  = 0; ///< Selection anchor (== mCaret when nothing is selected).
    bool        mFocused = false;
    // Modifier state seen through onKey (key-down auto-repeat refreshes the timestamps).
    bool   mCtrl = false, mShift = false;
    double mCtrlAt = -1, mShiftAt = -1;
    // Backspace can reach us twice: as VK_BACK and as a '\b' text character. Each side expects
    // at most one echo from the other within a short window (TextInput::now() seconds).
    double mBackKeyAt = -1, mBackCharAt = -1;
    bool   mBackKeyEcho = false, mBackCharEcho = false;
    double mLastClick = -1;
    core::HistoryCursor mHistoryCursor;

    // Render-time cache (render() is const; the box is single-threaded).
    mutable double             mLastTime       = 0;
    mutable double             mBlinkOrigin    = 0;
    mutable double             mLastRenderTime = 0;
    mutable std::size_t        mScroll         = 0; ///< Byte offset of the first visible character.
    mutable std::vector<Glyph> mGlyphs;             ///< Visible boundaries from mScroll.
    mutable std::string        mLayoutText;
    mutable std::size_t        mLayoutScroll = ~std::size_t{0};
    mutable float              mLayoutWidth  = -1;
};

} // namespace bei::ui
