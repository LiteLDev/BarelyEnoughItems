#pragma once

#include "core/model/ItemKey.h"
#include "core/state/Layout.h"

#include <cstdint>
#include <string>
#include <vector>

class MinecraftUIRenderContext;
class ItemStack;

namespace bei::ui {

/// RGBA color, components in [0,1].
struct Color {
    float r = 1, g = 1, b = 1, a = 1;

    static constexpr Color rgb(std::uint32_t hex, float alpha = 1.f) {
        return {((hex >> 16) & 0xFF) / 255.f, ((hex >> 8) & 0xFF) / 255.f, (hex & 0xFF) / 255.f, alpha};
    }
};

/// JEI-like palette shared by all panels so the UI looks consistent.
namespace palette {
inline constexpr Color PANEL_BG      = Color::rgb(0xC6C6C6);
inline constexpr Color PANEL_BORDER  = Color::rgb(0x373737);
inline constexpr Color PANEL_LIGHT   = Color::rgb(0xFFFFFF);
inline constexpr Color SLOT_BG       = Color::rgb(0x8B8B8B);
inline constexpr Color HOVER         = Color::rgb(0xFFFFFF, 0.5f);
inline constexpr Color TEXT          = Color::rgb(0x404040);
inline constexpr Color TEXT_LIGHT    = Color::rgb(0xFFFFFF);
inline constexpr Color TEXT_DIM      = Color::rgb(0xA0A0A0);
inline constexpr Color MISSING       = Color::rgb(0xFF0000, 0.4f);
inline constexpr Color TOOLTIP_BG    = Color::rgb(0x100010, 0.94f);
inline constexpr Color TOOLTIP_EDGE  = Color::rgb(0x5000FF, 0.6f);
inline constexpr Color HIGHLIGHT     = Color::rgb(0x80FF80, 0.5f);
} // namespace palette

enum class TextAlign { Left, Center, Right };

/// Thin batching wrapper over MinecraftUIRenderContext.
///
/// The game's UI renderer needs a strict order: rectangles -> flushImages, item icons,
/// text -> flushText. Callers may record in any order; flush() replays commands layer by
/// layer (Background, Items, Foreground, Text). Call flush() once per "z band"; a tooltip
/// drawn after a flush() appears above everything recorded before it.
///
/// All coordinates are GUI units (the same space as the vanilla UI's controls).
/// Only valid inside a UI render callback; never keep a Painter across frames.
class Painter {
public:
    enum class Layer { Background, Items, Foreground, Text };

    explicit Painter(MinecraftUIRenderContext& ctx);
    ~Painter(); // flushes
    Painter(Painter const&)            = delete;
    Painter& operator=(Painter const&) = delete;

    void fillRect(core::Rect const& r, Color c, Layer layer = Layer::Background);
    void outlineRect(core::Rect const& r, Color c, float thickness = 1, Layer layer = Layer::Background);
    /// Vanilla-style bevelled panel (light top-left edge, dark bottom-right edge).
    void panel(core::Rect const& r);
    /// Vanilla-style recessed 18x18 slot background at (x,y).
    void slot(float x, float y, float size = 18);

    /// Draw an item icon (16x16 * scale) with its top-left at (x,y). Unknown items draw nothing.
    void item(core::ItemKey const& key, float x, float y, float scale = 1, float alpha = 1);
    /// Same, for a stack the caller owns (e.g. from the player's inventory).
    void itemStack(ItemStack const& stack, float x, float y, float scale = 1, float alpha = 1);
    /// Stack count in the bottom-right corner of a 16x16 icon at (x,y); nothing for count <= 1.
    void count(int count, float x, float y, float scale = 1);

    void  text(std::string const& s, float x, float y, Color c = palette::TEXT, float scale = 1, TextAlign align = TextAlign::Left, float width = 0);
    [[nodiscard]] float textWidth(std::string const& s, float scale = 1) const;
    [[nodiscard]] float lineHeight(float scale = 1) const { return 9 * scale; }

    /// Textured quad from a resource-pack path such as "textures/ui/arrow".
    void texture(std::string const& path, core::Rect const& r, float alpha = 1);
    /// Nine-slice a `texW`x`texH` texture with a `slice`-pixel border into `r` (corners keep
    /// their size, edges and center stretch).
    void nineSlice(
        std::string const& path,
        core::Rect const&  r,
        float              texW,
        float              texH,
        float              slice,
        Layer              layer = Layer::Background
    );

    /// Replay all recorded commands in layer order.
    void flush();

    /// Screen size in GUI units.
    [[nodiscard]] core::Rect screen() const;
    [[nodiscard]] MinecraftUIRenderContext& raw() const { return mCtx; }

private:
    struct Cmd;
    MinecraftUIRenderContext& mCtx;
    std::vector<Cmd>*         mCmds; // pimpl-ish to keep Cmd out of the header
};

} // namespace bei::ui
