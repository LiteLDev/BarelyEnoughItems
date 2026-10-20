// Scaffold implementation: plain box with lines. Unit "Widgets" polishes it.
#include "ui/widgets/Tooltip.h"

#include "game/items/ItemCatalog.h"
#include "ui/gfx/Painter.h"

#include <algorithm>

namespace bei::ui {

void drawTooltip(Painter& p, std::vector<TooltipLine> const& lines, float x, float y, FrameInfo const& frame) {
    if (lines.empty()) return;
    float w = 0;
    for (auto const& l : lines) w = std::max(w, p.textWidth(l.text));
    float h  = static_cast<float>(lines.size()) * (p.lineHeight() + 1) + 4;
    float bx = x + 12, by = y - 12;
    if (bx + w + 8 > frame.screen.right()) bx = x - w - 16;
    by = std::clamp(by, 2.f, std::max(2.f, frame.screen.bottom() - h - 2));
    p.fillRect({bx, by, w + 8, h}, palette::TOOLTIP_BG, Painter::Layer::Foreground);
    p.outlineRect({bx, by, w + 8, h}, palette::TOOLTIP_EDGE, 1, Painter::Layer::Foreground);
    float ly = by + 3;
    for (auto const& l : lines) {
        p.text(l.text, bx + 4, ly, Color::rgb(l.rgb));
        ly += p.lineHeight() + 1;
    }
}

void drawItemTooltip(
    Painter&                        p,
    core::ItemKey const&            item,
    float                           x,
    float                           y,
    FrameInfo const&                frame,
    std::vector<TooltipLine> const& extra
) {
    std::vector<TooltipLine> lines{
        {game::ItemCatalog::get().displayName(item), 0xFFFFFF}
    };
    lines.insert(lines.end(), extra.begin(), extra.end());
    lines.push_back({item.toString(), 0x808080});
    drawTooltip(p, lines, x, y, frame);
}

} // namespace bei::ui
