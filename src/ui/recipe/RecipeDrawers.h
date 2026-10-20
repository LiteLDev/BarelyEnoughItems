#pragma once

// Internal to the recipe screen: per-LayoutKind measuring and drawing of one recipe.

#include "core/model/ItemKey.h"
#include "core/model/RecipeView.h"
#include "core/recipe/RecipeIndex.h"
#include "core/state/Layout.h"

#include <string>
#include <vector>

namespace bei::ui {
class Painter;
}

namespace bei::ui::recipe {

enum class SlotRole { Input, Output, Catalyst };

/// A slot drawn in the last frame (for hover, clicks and itemAt).
struct SlotHit {
    core::Rect    rect;
    core::ItemKey item;             ///< Item shown in the slot right now.
    int           alternatives = 0; ///< Number of accepted items (cycling when > 1).
    SlotRole      role         = SlotRole::Input;
    std::string   recipeId;         ///< Recipe the slot belongs to (empty for workstations).
};

/// A non-item area with a tooltip (shapeless icon, buttons...).
struct HintHit {
    core::Rect  rect;
    std::string text;
};

/// Everything a drawer needs besides the recipe itself.
struct DrawContext {
    Painter&                    painter;
    double                      time   = 0;
    float                       mouseX = -1, mouseY = -1;
    core::ItemKey const*        focus  = nullptr; ///< Focused item (may be null for a category browse).
    core::LookupMode            mode   = core::LookupMode::Recipes;
    std::vector<int> const*     missingInputs = nullptr; ///< Indices into RecipeView::inputs to tint red.
    std::vector<SlotHit>*       slots  = nullptr;
    std::vector<HintHit>*       hints  = nullptr;
};

struct Size {
    float w = 0, h = 0;
};

/// Size of the drawing of `recipe` (without its note line).
[[nodiscard]] Size measure(core::LayoutKind kind, core::RecipeView const& recipe);

/// Draw `recipe` with its top-left corner at (x,y). `cell` is the common cell size of the page,
/// used to center the drawing horizontally and place the note line at the bottom.
void draw(core::LayoutKind kind, core::RecipeView const& recipe, float x, float y, Size cell, DrawContext& ctx);

/// Height of the note line below a recipe drawing (0 when the recipe has no note).
[[nodiscard]] float noteHeight(core::RecipeView const& recipe);
/// Localized note text (the note is a lang key or a literal).
[[nodiscard]] std::string noteText(core::RecipeView const& recipe);

/// Item shown for an ingredient now: the focus if the slot accepts it, otherwise the
/// alternatives cycle once per second.
[[nodiscard]] core::ItemKey displayed(core::IngredientView const& ing, SlotRole role, DrawContext const& ctx, bool* focused = nullptr);

/// Draw one ingredient slot (18x18, or 26x26 when `big`) and record it in ctx.slots.
void ingredientSlot(
    DrawContext&                ctx,
    core::IngredientView const& ing,
    float                       x,
    float                       y,
    SlotRole                    role,
    std::string const&          recipeId,
    bool                        big     = false,
    bool                        missing = false
);

// Simple pixel-art shapes drawn with rectangles (no texture dependencies).
void drawArrowRight(Painter& p, float x, float y); ///< 22x15
void drawArrowDown(Painter& p, float x, float y);  ///< 11x12
void drawArrowLeftSmall(Painter& p, float x, float y, bool hover);  ///< 9x9 page button glyph
void drawArrowRightSmall(Painter& p, float x, float y, bool hover); ///< 9x9 page button glyph
void drawFlame(Painter& p, float x, float y);      ///< 14x14
void drawShapeless(Painter& p, float x, float y);  ///< 9x9

/// "{}" placeholder substitution for translated strings.
[[nodiscard]] std::string substitute(std::string text, std::string const& arg);

} // namespace bei::ui::recipe
