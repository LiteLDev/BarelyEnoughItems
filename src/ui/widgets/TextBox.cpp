// Scaffold implementation: append/backspace only, no caret movement, no keyboard ownership.
// Unit "Widgets" completes it.
#include "ui/widgets/TextBox.h"

#include "ui/gfx/Painter.h"

namespace bei::ui {

void TextBox::render(Painter& p, double time) const {
    p.fillRect(rect, mFocused ? Color::rgb(0xFFFFFF) : Color::rgb(0xA0A0A0));
    p.fillRect({rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2}, Color::rgb(0x000000));
    float ty = rect.y + (rect.h - p.lineHeight()) / 2;
    if (mText.empty() && !mFocused) {
        p.text(placeholder, rect.x + 4, ty, palette::TEXT_DIM);
    } else {
        bool blink = static_cast<long long>(time * 2) % 2 == 0;
        p.text(mText + (mFocused && blink ? "_" : ""), rect.x + 4, ty, palette::TEXT_LIGHT);
    }
}

bool TextBox::onMouseButton(bool right, bool down, float x, float y) {
    if (!down) return rect.contains(x, y);
    if (!rect.contains(x, y)) {
        setFocused(false);
        return false;
    }
    setFocused(true);
    if (right) setText({}, true);
    return true;
}

bool TextBox::onKey(int vk, bool down) {
    if (!mFocused) return false;
    if (down && vk == 0x08 /*VK_BACK*/ && !mText.empty()) {
        // Drop the last UTF-8 code point.
        std::size_t n = mText.size() - 1;
        while (n > 0 && (static_cast<unsigned char>(mText[n]) & 0xC0) == 0x80) --n;
        setText(mText.substr(0, n), true);
    }
    if (down && (vk == 0x0D /*VK_RETURN*/ || vk == 0x1B /*VK_ESCAPE*/)) setFocused(false);
    return true;
}

bool TextBox::onChar(std::string const& utf8) {
    if (!mFocused) return false;
    if (!utf8.empty() && static_cast<unsigned char>(utf8[0]) >= 0x20) setText(mText + utf8, true);
    return true;
}

void TextBox::setText(std::string text, bool notify) {
    mText  = std::move(text);
    mCaret = mText.size();
    if (notify && onChange) onChange(mText);
}

void TextBox::setFocused(bool focused) { mFocused = focused; }

} // namespace bei::ui
