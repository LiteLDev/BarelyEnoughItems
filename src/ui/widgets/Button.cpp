#include "ui/widgets/Button.h"

#include "ui/gfx/Painter.h"

#include <cmath>

namespace bei::ui {

namespace {
struct Look {
    Color outline, light, dark, face, text;
};

constexpr Look NORMAL{
    Color::rgb(0x000000),
    Color::rgb(0xA9A9A9),
    Color::rgb(0x484848),
    Color::rgb(0x6F6F6F),
    Color::rgb(0xFFFFFF)
};
constexpr Look
    HOVER{Color::rgb(0x000000), Color::rgb(0xBAC3F5), Color::rgb(0x4A5490), Color::rgb(0x7E88BF), Color::rgb(0xFFFFA0)};
constexpr Look PRESSED{
    Color::rgb(0x000000),
    Color::rgb(0x383838),
    Color::rgb(0x8A8A8A),
    Color::rgb(0x565656),
    Color::rgb(0xFFFFFF)
};
constexpr Look DISABLED{
    Color::rgb(0x000000),
    Color::rgb(0x363636),
    Color::rgb(0x242424),
    Color::rgb(0x2C2C2C),
    Color::rgb(0xA0A0A0)
};

constexpr float GLYPH = 7; // glyph box size in GUI units
} // namespace

void drawGlyph(Painter& p, Glyph glyph, core::Rect const& area, Color c) {
    float gx = std::floor(area.x + (area.w - GLYPH) / 2);
    float gy = std::floor(area.y + (area.h - GLYPH) / 2);
    switch (glyph) {
    case Glyph::None:
        break;
    case Glyph::ArrowRight:
        for (int i = 0; i < 4; ++i) p.fillRect({gx + 2 + i, gy + i, 1, GLYPH - 2.f * i}, c);
        break;
    case Glyph::ArrowLeft:
        for (int i = 0; i < 4; ++i) p.fillRect({gx + 5 - i, gy + i, 1, GLYPH - 2.f * i}, c);
        break;
    case Glyph::ArrowDown:
        for (int i = 0; i < 4; ++i) p.fillRect({gx + i, gy + 2 + i, GLYPH - 2.f * i, 1}, c);
        break;
    case Glyph::ArrowUp:
        for (int i = 0; i < 4; ++i) p.fillRect({gx + i, gy + 5 - i, GLYPH - 2.f * i, 1}, c);
        break;
    case Glyph::Plus:
        p.fillRect({gx, gy + 3, GLYPH, 1}, c);
        p.fillRect({gx + 3, gy, 1, 3}, c);
        p.fillRect({gx + 3, gy + 4, 1, 3}, c);
        break;
    case Glyph::Minus:
        p.fillRect({gx, gy + 3, GLYPH, 1}, c);
        break;
    case Glyph::Cross:
        for (int i = 0; i < 7; ++i) {
            p.fillRect({gx + i, gy + i, 1, 1}, c);
            if (i != 3) p.fillRect({gx + 6 - i, gy + i, 1, 1}, c);
        }
        break;
    case Glyph::Check:
        for (int i = 0; i < 3; ++i) p.fillRect({gx + i, gy + 3 + i, 1, 2}, c);
        for (int i = 0; i < 4; ++i) p.fillRect({gx + 3 + i, gy + 4 - i, 1, 2}, c);
        break;
    }
}

void Button::render(Painter& p, float mx, float my) const {
    if (rect.empty()) return;
    bool        hover = enabled && rect.contains(mx, my);
    Look const& look  = !enabled ? DISABLED : toggled ? PRESSED : hover ? HOVER : NORMAL;
    auto const& r     = rect;

    p.fillRect(r, look.outline);
    p.fillRect({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, look.dark);  // bottom/right edge
    p.fillRect({r.x + 1, r.y + 1, r.w - 3, r.h - 3}, look.light); // top/left edge
    p.fillRect({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, look.face);

    // Content: [icon or glyph] [label], centred; pressed buttons shift by one unit.
    float off    = toggled && enabled ? 1.f : 0.f;
    bool  hasImg = !icon.empty() || glyph != Glyph::None;
    float imgW   = !icon.empty() ? 16.f : glyph != Glyph::None ? GLYPH : 0.f;
    float textW  = label.empty() ? 0.f : p.textWidth(label);
    float gap    = hasImg && !label.empty() ? 2.f : 0.f;
    float x      = std::floor(r.x + (r.w - (imgW + gap + textW)) / 2) + off;
    float cy     = r.y + r.h / 2 + off;

    if (!icon.empty()) {
        p.item(icon, x, std::floor(cy - 8), 1, enabled ? 1.f : 0.5f);
    } else if (glyph != Glyph::None) {
        // Drop shadow like the vanilla font, then the glyph.
        drawGlyph(p, glyph, {x + 1, std::floor(cy - GLYPH / 2) + 1, GLYPH, GLYPH}, Color::rgb(0x000000, 0.35f));
        drawGlyph(p, glyph, {x, std::floor(cy - GLYPH / 2), GLYPH, GLYPH}, look.text);
    }
    if (!label.empty()) {
        p.text(label, x + imgW + gap, std::floor(cy - p.lineHeight() / 2) + 1, look.text);
    }
}

} // namespace bei::ui
