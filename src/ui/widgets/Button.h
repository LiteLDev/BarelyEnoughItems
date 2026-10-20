#pragma once

#include "core/model/ItemKey.h"
#include "core/state/Layout.h"

#include <cstdint>
#include <optional>
#include <string>

namespace bei::ui {

class Painter;
struct Color;

/// Simple vector icons drawn with rectangles (the UI font has no reliable arrow glyphs).
enum class Glyph { None, ArrowLeft, ArrowRight, ArrowUp, ArrowDown, Plus, Minus, Cross, Check, Wrench };

/// Draw `glyph` centred in `area` (pixel-art style, about 7x7 GUI units).
void drawGlyph(Painter& painter, Glyph glyph, core::Rect const& area, Color color);

/// Small vanilla-style button with a text label and/or an item icon or glyph.
///
/// Look: black outline, light top-left / dark bottom-right bevel, grey face; blue-ish face and
/// yellow text on hover; flat dark face and grey text when disabled; sunken when `toggled`.
struct Button {
    core::Rect    rect;
    std::string   label;
    core::ItemKey icon;
    bool          enabled = true;
    Glyph         glyph   = Glyph::None; ///< Drawn when there is no item icon.
    bool          toggled = false;       ///< Toggle buttons: draw pressed in.
    /// Tint (RGB) replacing the grey face, e.g. red for the settings button in cheat mode.
    /// Hover lightens it; the bevel is derived from it. Ignored while disabled.
    std::optional<std::uint32_t> tint;

    void               render(Painter& painter, float mouseX, float mouseY) const;
    [[nodiscard]] bool hit(float x, float y) const noexcept { return enabled && rect.contains(x, y); }
    [[nodiscard]] bool hovered(float x, float y) const noexcept { return rect.contains(x, y); }
};

/// Square button showing only a glyph, e.g. the ◀ ▶ page arrows.
struct IconButton : Button {
    IconButton() = default;
    IconButton(core::Rect r, Glyph g) {
        rect  = r;
        glyph = g;
    }
};

} // namespace bei::ui
