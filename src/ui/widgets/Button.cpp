#include "ui/widgets/Button.h"

#include "ui/gfx/Painter.h"

namespace bei::ui {

void Button::render(Painter& p, float mx, float my) const {
    bool hover = enabled && rect.contains(mx, my);
    p.fillRect(rect, Color::rgb(0x000000));
    p.fillRect({rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2}, hover ? Color::rgb(0x7F89BF) : Color::rgb(0x6F6F6F));
    if (!icon.empty()) p.item(icon, rect.x + (rect.w - 16) / 2, rect.y + (rect.h - 16) / 2);
    if (!label.empty()) {
        p.text(
            label,
            rect.x + rect.w / 2,
            rect.y + (rect.h - p.lineHeight()) / 2,
            enabled ? palette::TEXT_LIGHT : palette::TEXT_DIM,
            1,
            TextAlign::Center
        );
    }
}

} // namespace bei::ui
