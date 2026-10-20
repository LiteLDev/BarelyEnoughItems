#pragma once

#include "core/model/ItemKey.h"
#include "core/state/Layout.h"

#include <string>

namespace bei::ui {

class Painter;

/// Small vanilla-style button with a text label and/or an item icon.
struct Button {
    core::Rect    rect;
    std::string   label;
    core::ItemKey icon;
    bool          enabled = true;

    void               render(Painter& painter, float mouseX, float mouseY) const;
    [[nodiscard]] bool hit(float x, float y) const noexcept { return enabled && rect.contains(x, y); }
};

} // namespace bei::ui
