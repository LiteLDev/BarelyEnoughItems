#include "ui/widgets/TextBox.h"

#include "ui/gfx/Painter.h"
#include "ui/widgets/Clipboard.h"
#include "ui/widgets/TextInput.h"
#include "ui/widgets/Utf8.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#pragma comment(lib, "user32.lib") // clipboard and key state

#include <algorithm>
#include <cmath>

namespace bei::ui {

namespace {
constexpr float PAD = 4; // horizontal text padding inside the border

constexpr Color BORDER_FOCUSED = Color::rgb(0xFFFFFF);
constexpr Color BORDER_IDLE    = Color::rgb(0xA0A0A0);
constexpr Color FIELD_BG       = Color::rgb(0x000000);
constexpr Color PLACEHOLDER    = Color::rgb(0x707070);
constexpr Color SELECTION      = Color::rgb(0x3D5AFE, 0.75f);
constexpr Color CARET          = Color::rgb(0xE0E0E0);

constexpr double BLINK_PERIOD         = 1.2;  // seconds per on/off cycle
constexpr double BLINK_ON             = 0.7;  // visible part of the cycle (starts visible after edits)
constexpr double DOUBLE_CLICK_SECONDS = 0.35; // seconds
constexpr double BACKSPACE_ECHO       = 0.25; // a '\b' char this soon after VK_BACK is the key's own echo
constexpr double MODIFIER_GRACE       = 0.6;  // seconds a seen Ctrl/Shift key-down stays valid

bool keyHeld(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
} // namespace

TextBox::~TextBox() { TextInput::forget(*this); }

TextBox::TextBox(TextBox const& o)
: rect(o.rect),
  placeholder(o.placeholder),
  onChange(o.onChange),
  maxLength(o.maxLength),
  textColor(o.textColor),
  mText(o.mText),
  mCaret(o.mText.size()),
  mAnchor(o.mText.size()) {}

TextBox& TextBox::operator=(TextBox const& o) {
    if (this == &o) return *this;
    setFocused(false);
    rect        = o.rect;
    placeholder = o.placeholder;
    onChange    = o.onChange;
    maxLength   = o.maxLength;
    textColor   = o.textColor;
    mText       = o.mText;
    mCaret = mAnchor = mText.size();
    mScroll          = 0;
    mLayoutScroll    = ~std::size_t{0};
    return *this;
}

// --- Rendering -------------------------------------------------------------------------------

void TextBox::layout(Painter& p) const {
    float inner = std::max(0.f, rect.w - 2 * PAD);
    auto  width = [&](std::size_t from, std::size_t to) {
        return to <= from ? 0.f : p.textWidth(mText.substr(from, to - from));
    };

    // Keep the caret visible, and use the free space on the right when text was deleted.
    mScroll = utf8::floorBoundary(mText, std::min(mScroll, mText.size()));
    if (mCaret < mScroll) mScroll = mCaret;
    while (mScroll < mCaret && width(mScroll, mCaret) > inner) mScroll = utf8::next(mText, mScroll);
    while (mScroll > 0 && width(utf8::prev(mText, mScroll), mText.size()) <= inner)
        mScroll = utf8::prev(mText, mScroll);

    if (mLayoutText == mText && mLayoutScroll == mScroll && mLayoutWidth == inner) return;
    mLayoutText   = mText;
    mLayoutScroll = mScroll;
    mLayoutWidth  = inner;
    mGlyphs.clear();
    mGlyphs.push_back({mScroll, 0});
    for (std::size_t pos = mScroll; pos < mText.size();) {
        pos     = utf8::next(mText, pos);
        float x = width(mScroll, pos);
        if (x > inner) break;
        mGlyphs.push_back({pos, x});
    }
}

void TextBox::render(Painter& p, double time) const {
    mLastTime       = time;
    mLastRenderTime = TextInput::now();

    p.fillRect(rect, mFocused ? BORDER_FOCUSED : BORDER_IDLE);
    p.fillRect({rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2}, FIELD_BG);
    layout(p);

    float x0 = rect.x + PAD;
    float ty = rect.y + std::floor((rect.h - p.lineHeight()) / 2) + 1;
    float lh = p.lineHeight();

    if (mText.empty()) {
        if (!placeholder.empty()) p.text(placeholder, x0, ty, PLACEHOLDER);
    } else {
        auto glyphX = [&](std::size_t offset) {
            if (offset <= mGlyphs.front().offset) return mGlyphs.front().x;
            for (auto const& g : mGlyphs) {
                if (g.offset >= offset) return g.x;
            }
            return mGlyphs.back().x;
        };
        std::size_t visibleEnd = mGlyphs.back().offset;
        if (mFocused && hasSelection()) {
            float a = glyphX(std::min(mCaret, mAnchor));
            float b = glyphX(std::max(mCaret, mAnchor));
            if (std::max(mCaret, mAnchor) > visibleEnd) b = std::min(rect.w - 2 * PAD, b + PAD);
            p.fillRect({x0 + a, ty - 1, b - a, lh + 1}, SELECTION);
        }
        p.text(mText.substr(mScroll, visibleEnd - mScroll), x0, ty, Color::rgb(textColor));
    }

    if (mFocused && std::fmod(time - mBlinkOrigin, BLINK_PERIOD) < BLINK_ON) {
        float cx = 0;
        for (auto const& g : mGlyphs) {
            if (g.offset == mCaret) cx = g.x;
        }
        p.fillRect({x0 + cx - (cx > 0 ? 1.f : 0.f), ty - 1, 1, lh + 1}, CARET, Painter::Layer::Foreground);
    }
}

// --- Editing primitives ----------------------------------------------------------------------

std::size_t TextBox::offsetAt(float x) const {
    float rel = x - (rect.x + PAD);
    if (mGlyphs.empty()) return mText.size();
    std::size_t best     = mGlyphs.front().offset;
    float       bestDist = std::abs(rel - mGlyphs.front().x);
    for (auto const& g : mGlyphs) {
        float d = std::abs(rel - g.x);
        if (d < bestDist) best = g.offset, bestDist = d;
    }
    return std::min(best, mText.size());
}

void TextBox::setCaret(std::size_t pos, bool extend) {
    mCaret = utf8::floorBoundary(mText, std::min(pos, mText.size()));
    if (!extend) mAnchor = mCaret;
    restartBlink();
}

void TextBox::changed() {
    restartBlink();
    if (onChange) onChange(mText);
}

void TextBox::eraseRange(std::size_t from, std::size_t to) {
    if (from > to) std::swap(from, to);
    to = std::min(to, mText.size());
    if (from >= to) return;
    mText.erase(from, to - from);
    mCaret = mAnchor = from;
}

void TextBox::deleteSelection() { eraseRange(mCaret, mAnchor); }

std::string TextBox::selectedText() const {
    auto a = std::min(mCaret, mAnchor), b = std::max(mCaret, mAnchor);
    return mText.substr(a, b - a);
}

void TextBox::selectAll() {
    mAnchor = 0;
    mCaret  = mText.size();
    restartBlink();
}

void TextBox::insert(std::string const& utf8) {
    std::string clean  = utf8::sanitizeLine(utf8);
    std::size_t selLen = utf8::length(selectedText());
    std::size_t room   = maxLength > utf8::length(mText) - selLen ? maxLength - (utf8::length(mText) - selLen) : 0;
    clean.resize(utf8::prefixBytes(clean, room));
    if (clean.empty()) return;
    deleteSelection();
    mText.insert(mCaret, clean);
    mCaret = mAnchor = mCaret + clean.size();
    changed();
}

void TextBox::setText(std::string text, bool notify) {
    mText  = std::move(text);
    mCaret = mAnchor = mText.size();
    mScroll          = 0;
    restartBlink();
    if (notify && onChange) onChange(mText);
}

void TextBox::setFocused(bool focused) {
    if (focused == mFocused) return;
    mFocused = focused;
    mCtrl = mShift = false;
    mBackKeyEcho = mBackCharEcho = false;
    restartBlink();
    if (focused) {
        mLastRenderTime = TextInput::now(); // may be focused before its first render
        TextInput::acquire(*this);
    } else {
        mAnchor = mCaret;
        TextInput::release(*this);
    }
}

// --- Input -----------------------------------------------------------------------------------

// Events are handled a little after they happened (client thread), so a modifier may already be
// released when its combination is processed: trust a recent key-down we saw as well as the live
// state. A lost key-up (focus change) expires instead of sticking.
bool TextBox::ctrlDown() const { return keyHeld(VK_CONTROL) || (mCtrl && TextInput::now() - mCtrlAt < MODIFIER_GRACE); }

bool TextBox::shiftDown() const {
    return keyHeld(VK_SHIFT) || (mShift && TextInput::now() - mShiftAt < MODIFIER_GRACE);
}

bool TextBox::onMouseButton(bool right, bool down, float x, float y) {
    bool inside = rect.contains(x, y);
    if (!down) return inside && mFocused;
    if (!inside) {
        setFocused(false);
        return false;
    }
    if (right) {
        // JEI: right click clears the field.
        setFocused(true);
        if (!mText.empty()) setText({}, true);
        return true;
    }
    bool wasFocused = mFocused;
    setFocused(true);
    double now         = TextInput::now();
    bool   doubleClick = wasFocused && mLastClick >= 0 && now - mLastClick < DOUBLE_CLICK_SECONDS;
    mLastClick         = doubleClick ? -1 : now;
    if (doubleClick) selectAll();
    else setCaret(offsetAt(x), wasFocused && shiftDown());
    return true;
}

bool TextBox::onKey(int vk, bool down) {
    if (!mFocused) return false;
    switch (vk) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        mCtrl   = down;
        mCtrlAt = TextInput::now();
        return true;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        mShift   = down;
        mShiftAt = TextInput::now();
        return true;
    default:
        break;
    }
    if (!down) return true; // swallow releases too so the game never sees half a key press

    bool   alt   = keyHeld(VK_MENU); // AltGr reports Ctrl+Alt; leave it to the text path
    bool   ctrl  = ctrlDown() && !alt;
    bool   shift = shiftDown();
    double now   = TextInput::now();

    switch (vk) {
    case VK_LEFT:
        if (hasSelection() && !shift) setCaret(std::min(mCaret, mAnchor), false);
        else setCaret(ctrl ? utf8::wordLeft(mText, mCaret) : utf8::prev(mText, mCaret), shift);
        break;
    case VK_RIGHT:
        if (hasSelection() && !shift) setCaret(std::max(mCaret, mAnchor), false);
        else setCaret(ctrl ? utf8::wordRight(mText, mCaret) : utf8::next(mText, mCaret), shift);
        break;
    case VK_HOME:
        setCaret(0, shift);
        break;
    case VK_END:
        setCaret(mText.size(), shift);
        break;
    case VK_BACK:
        if (mBackCharEcho && now - mBackCharAt <= BACKSPACE_ECHO) {
            mBackCharEcho = false; // the text path already applied this Backspace
            break;
        }
        mBackCharEcho = false;
        mBackKeyEcho  = true;
        mBackKeyAt    = now;
        if (hasSelection()) deleteSelection();
        else if (mCaret == 0) break;
        else eraseRange(ctrl ? utf8::wordLeft(mText, mCaret) : utf8::prev(mText, mCaret), mCaret);
        changed();
        break;
    case VK_DELETE:
        if (hasSelection()) deleteSelection();
        else if (mCaret >= mText.size()) break;
        else eraseRange(mCaret, ctrl ? utf8::wordRight(mText, mCaret) : utf8::next(mText, mCaret));
        changed();
        break;
    case VK_RETURN:
    case VK_ESCAPE:
        setFocused(false);
        break;
    case 'A':
        if (ctrl) selectAll();
        break;
    case 'C':
        if (ctrl && hasSelection()) clipboard::setText(selectedText());
        break;
    case 'X':
        if (ctrl && hasSelection() && clipboard::setText(selectedText())) { // never lose text
            deleteSelection();
            changed();
        }
        break;
    case 'V':
        if (ctrl) insert(clipboard::getText());
        break;
    default:
        break;
    }
    return true;
}

bool TextBox::onChar(std::string const& utf8) {
    if (!mFocused) return false;
    // Insert printable runs; honour '\b' (IMEs rewrite their composition with it) unless it is
    // just the echo of a Backspace key we already handled. Other control characters (Ctrl+A
    // arrives as \x01, Enter as \r, ...) are handled through onKey.
    std::size_t runStart = 0;
    for (std::size_t i = 0; i <= utf8.size();) {
        bool end = i == utf8.size();
        if (end || utf8[i] == '\b') {
            if (i > runStart) insert(utf8.substr(runStart, i - runStart));
            if (end) break;
            double now = TextInput::now();
            if (mBackKeyEcho && now - mBackKeyAt <= BACKSPACE_ECHO) {
                mBackKeyEcho = false; // echo of a Backspace key already handled in onKey
            } else {
                mBackKeyEcho  = false;
                mBackCharEcho = true;
                mBackCharAt   = now;
                if (hasSelection() || mCaret > 0) {
                    if (hasSelection()) deleteSelection();
                    else eraseRange(utf8::prev(mText, mCaret), mCaret);
                    changed();
                }
            }
            runStart = ++i;
            continue;
        }
        ++i;
    }
    return true;
}

} // namespace bei::ui
