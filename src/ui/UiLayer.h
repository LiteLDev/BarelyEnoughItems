#pragma once

#include "core/model/ItemKey.h"
#include "core/state/Layout.h"

#include <optional>
#include <string>

namespace bei::ui {

class Painter;

/// Per-frame information about the screen BEI draws on. GUI units throughout.
struct FrameInfo {
    core::Rect  screen;                 ///< Whole screen.
    core::Rect  container;              ///< Vanilla container panel (empty if unknown / not a container).
    std::string screenName;             ///< e.g. "inventory_screen", "crafting_screen", "hud_screen".
    bool        isContainerScreen = false;
    bool        inLevel           = false;
    float       mouseX = -1, mouseY = -1; ///< Last known cursor position (-1 when unknown).
    double      time   = 0;               ///< Seconds since mod start (animations, cycling).
};

enum class MouseButton { Left, Right, Middle };

/// Something BEI draws and that can receive input. Layers are registered once at static-init
/// time (BEI_REGISTER_UI_LAYER) and live for the whole session.
///
/// Rendering happens on the client thread inside the UI render pass of the focused screen.
/// Input callbacks are delivered on the client thread by input::InputRouter, in descending
/// zOrder, only to layers that were visible in the last rendered frame; returning true
/// consumes the event (vanilla UI does not see it).
class IUiLayer {
public:
    virtual ~IUiLayer() = default;

    [[nodiscard]] virtual char const* name() const   = 0;
    [[nodiscard]] virtual int         zOrder() const = 0; ///< Higher draws later, gets input first.
    [[nodiscard]] virtual bool        visible(FrameInfo const& frame) const = 0;
    virtual void                      render(Painter& painter, FrameInfo const& frame) = 0;

    virtual bool onMouseButton(MouseButton, bool /*down*/, float /*x*/, float /*y*/) { return false; }
    virtual bool onScroll(float /*x*/, float /*y*/, float /*delta*/) { return false; }
    /// Windows virtual-key code.
    virtual bool onKey(int /*vk*/, bool /*down*/) { return false; }
    /// Typed text (UTF-8), only delivered while some layer has text focus.
    virtual bool onChar(std::string const& /*utf8*/) { return false; }
    /// True if this layer currently owns keyboard text input (suppresses hotkeys & game keys).
    [[nodiscard]] virtual bool wantsTextInput() const { return false; }
    /// True if the point is over this layer (blocks vanilla hover/click below it).
    [[nodiscard]] virtual bool contains(float /*x*/, float /*y*/) const { return false; }
    /// Item under the point, for R/U/A hotkeys and tooltips.
    [[nodiscard]] virtual std::optional<core::ItemKey> itemAt(float /*x*/, float /*y*/) const { return std::nullopt; }
};

} // namespace bei::ui
