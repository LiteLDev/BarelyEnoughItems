#pragma once

#include "core/model/ItemKey.h"
#include "ui/UiLayer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace bei::ui {

class Painter;

/// One line of tooltip text.
struct TooltipLine {
    std::string   text;
    std::uint32_t rgb = 0xFFFFFF;
};

/// Draw a vanilla-style tooltip next to the cursor position (x,y), kept inside the screen.
/// Call after everything else in the frame was flushed so it stays on top.
void drawTooltip(Painter& painter, std::vector<TooltipLine> const& lines, float x, float y, FrameInfo const& frame);

/// Item tooltip: localized name, extra lines (e.g. "Recipe: 3/9"), id + namespace in grey,
/// and the hotkey hint ("R: Recipes  U: Uses").
void drawItemTooltip(
    Painter&                        painter,
    core::ItemKey const&            item,
    float                           x,
    float                           y,
    FrameInfo const&                frame,
    std::vector<TooltipLine> const& extra = {}
);

} // namespace bei::ui
