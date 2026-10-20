#include "ui/recipe/RecipeDrawers.h"

#include "app/I18n.h"
#include "ui/gfx/Painter.h"

#include <algorithm>
#include <cmath>

namespace bei::ui::recipe {

namespace {
constexpr float SLOT = 18;
constexpr float BIG  = 26;

constexpr Color ARROW_COLOR = Color::rgb(0x8B8B8B);
constexpr Color ICON_COLOR  = Color::rgb(0x555555);

bool contains(std::vector<int> const* list, int value) {
    return list && std::find(list->begin(), list->end(), value) != list->end();
}

/// All catalysts merged into one cycling slot (fuel, blaze powder...).
core::IngredientView mergedCatalysts(core::RecipeView const& r) {
    if (r.catalysts.size() == 1) return r.catalysts.front();
    core::IngredientView merged;
    for (auto const& c : r.catalysts) {
        for (auto const& alt : c.alternatives) {
            if (std::find(merged.alternatives.begin(), merged.alternatives.end(), alt) == merged.alternatives.end()) {
                merged.alternatives.push_back(alt);
            }
        }
    }
    return merged;
}

core::IngredientView const& inputAt(core::RecipeView const& r, std::size_t i) {
    static core::IngredientView const empty;
    return i < r.inputs.size() ? r.inputs[i] : empty;
}

/// Lays out (and, when ctx is set, draws) one recipe at (ox,oy). Returns its size.
/// Measuring and drawing share this code so they can never disagree.
Size layout(core::LayoutKind kind, core::RecipeView const& r, float ox, float oy, DrawContext* ctx) {
    auto slot = [&](core::IngredientView const& ing, float x, float y, SlotRole role, bool big = false, int input = -1) {
        if (ctx) ingredientSlot(*ctx, ing, ox + x, oy + y, role, r.id, big, input >= 0 && contains(ctx->missingInputs, input));
    };
    auto arrow = [&](float x, float y) {
        if (ctx) drawArrowRight(ctx->painter, ox + x, oy + y);
    };
    // Outputs: one big slot, or a grid of small ones (`cols` wide) starting at (x,y).
    auto outputs = [&](float x, float y, int cols) -> Size {
        auto n = static_cast<int>(r.outputs.size());
        if (n == 0) return {};
        if (n == 1) {
            slot(r.outputs[0], x, y, SlotRole::Output, true);
            return {BIG, BIG};
        }
        cols     = std::clamp(cols, 1, n);
        int rows = (n + cols - 1) / cols;
        for (int i = 0; i < n; ++i) {
            slot(r.outputs[i], x + static_cast<float>(i % cols) * SLOT, y + static_cast<float>(i / cols) * SLOT, SlotRole::Output);
        }
        return {static_cast<float>(cols) * SLOT, static_cast<float>(rows) * SLOT};
    };

    switch (kind) {
    case core::LayoutKind::CraftingGrid: {
        auto n    = static_cast<int>(r.inputs.size());
        int  stride = r.width > 0 ? r.width : (n <= 1 ? 1 : n <= 4 ? 2 : 3); // row length of the inputs
        int  cols   = std::clamp(stride, 1, 3);
        int  rows   = r.height > 0 ? r.height : (n + stride - 1) / stride;
        rows      = std::clamp(rows, 1, 3);
        for (int gy = 0; gy < 3; ++gy) {
            for (int gx = 0; gx < 3; ++gx) {
                float x = static_cast<float>(gx) * SLOT, y = static_cast<float>(gy) * SLOT;
                int   i = gy * stride + gx;
                if (gx < cols && gy < rows && i < n) {
                    slot(r.inputs[i], x, y, SlotRole::Input, false, i);
                } else if (ctx) {
                    ctx->painter.slot(ox + x, oy + y);
                }
            }
        }
        arrow(60, (3 * SLOT - 15) / 2);
        // One big result; several results (rare) as small slots.
        Size out = r.outputs.size() > 1 ? outputs(88, 0, 2) : outputs(88, (3 * SLOT - BIG) / 2, 1);
        if (r.shapeless && ctx) {
            float ix = ox + 88 + BIG - 9, iy = oy + 1;
            drawShapeless(ctx->painter, ix, iy);
            if (ctx->hints) ctx->hints->push_back({{ix - 1, iy - 1, 11, 11}, app::tr("bei.recipe.shapeless", "Shapeless recipe")});
        }
        return {88 + std::max(out.w, BIG), 3 * SLOT};
    }
    case core::LayoutKind::Smelting: {
        slot(inputAt(r, 0), 0, 0, SlotRole::Input, false, 0);
        if (ctx) drawFlame(ctx->painter, ox + 2, oy + 20);
        if (!r.catalysts.empty()) slot(mergedCatalysts(r), 0, 36, SlotRole::Catalyst);
        arrow(24, 19.5f);
        Size out = outputs(52, 14, 2);
        return {52 + std::max(out.w, BIG), std::max(3 * SLOT, 14 + out.h)};
    }
    case core::LayoutKind::Brewing: {
        if (!r.catalysts.empty()) slot(mergedCatalysts(r), 0, 0, SlotRole::Catalyst);
        slot(inputAt(r, 1), 40, 0, SlotRole::Input, false, 1);
        if (ctx) drawArrowDown(ctx->painter, ox + 43.5f, oy + 20);
        slot(inputAt(r, 0), 40, 34, SlotRole::Input, false, 0);
        arrow(62, 35.5f);
        Size out = outputs(88, 30, 2);
        return {88 + std::max(out.w, BIG), std::max(56.f, 30 + out.h)};
    }
    case core::LayoutKind::Smithing: {
        for (int i = 0; i < 3; ++i) slot(inputAt(r, static_cast<std::size_t>(i)), static_cast<float>(i) * SLOT, 4, SlotRole::Input, false, i);
        arrow(60, 5.5f);
        Size out = outputs(88, 0, 2);
        return {88 + std::max(out.w, BIG), std::max(BIG, out.h)};
    }
    case core::LayoutKind::SingleInput: {
        auto  n    = static_cast<int>(r.outputs.size());
        float outH = n <= 1 ? BIG : static_cast<float>((n + 3) / 4) * SLOT;
        float h    = std::max(BIG, outH);
        slot(inputAt(r, 0), 0, (h - SLOT) / 2, SlotRole::Input, false, 0);
        arrow(24, (h - 15) / 2);
        Size out = outputs(52, n <= 1 ? (h - BIG) / 2 : 0, 4);
        return {52 + std::max(out.w, BIG), h};
    }
    case core::LayoutKind::Generic:
    default: {
        auto  n     = static_cast<int>(r.inputs.size());
        auto  m     = static_cast<int>(r.outputs.size());
        int   cols  = std::clamp(n, 1, 4);
        int   rows  = std::max(1, (n + cols - 1) / cols);
        float inW   = n == 0 ? 0 : static_cast<float>(cols) * SLOT;
        float inH   = static_cast<float>(rows) * SLOT;
        int   ocols = std::clamp(m, 1, 3);
        float outH  = m == 0 ? 0 : m == 1 ? BIG : static_cast<float>((m + ocols - 1) / ocols) * SLOT;
        float h     = std::max({inH, outH, SLOT});
        for (int i = 0; i < n; ++i) {
            slot(r.inputs[i], static_cast<float>(i % cols) * SLOT, (h - inH) / 2 + static_cast<float>(i / cols) * SLOT, SlotRole::Input, false, i);
        }
        float w = inW;
        if (m > 0) {
            float ax = n == 0 ? 0 : inW + 4;
            arrow(ax, (h - 15) / 2);
            Size out = outputs(ax + 28, (h - outH) / 2, ocols);
            w        = ax + 28 + out.w;
        }
        // Catalysts (if any) in a row below.
        if (!r.catalysts.empty()) {
            for (std::size_t i = 0; i < r.catalysts.size(); ++i) {
                slot(r.catalysts[i], static_cast<float>(i) * SLOT, h + 2, SlotRole::Catalyst);
            }
            w = std::max(w, static_cast<float>(r.catalysts.size()) * SLOT);
            h += 2 + SLOT;
        }
        return {std::max(w, SLOT), h};
    }
    }
}
} // namespace

std::string substitute(std::string text, std::string const& arg) {
    if (auto pos = text.find("{}"); pos != std::string::npos) text.replace(pos, 2, arg);
    return text;
}

Size measure(core::LayoutKind kind, core::RecipeView const& recipe) { return layout(kind, recipe, 0, 0, nullptr); }

float noteHeight(core::RecipeView const& recipe) { return recipe.note.empty() ? 0.f : 11.f; }

std::string noteText(core::RecipeView const& recipe) {
    if (recipe.note.empty()) return {};
    return app::tr(recipe.note, recipe.note);
}

void draw(core::LayoutKind kind, core::RecipeView const& recipe, float x, float y, Size cell, DrawContext& ctx) {
    Size  size = measure(kind, recipe);
    float ox   = std::floor(x + (cell.w - size.w) / 2);
    layout(kind, recipe, ox, y, &ctx);
    if (!recipe.note.empty()) {
        ctx.painter.text(noteText(recipe), x + cell.w / 2, y + size.h + 2, palette::TEXT, 1, TextAlign::Center);
    }
}

core::ItemKey displayed(core::IngredientView const& ing, SlotRole role, DrawContext const& ctx, bool* focused) {
    if (focused) *focused = false;
    if (ing.alternatives.empty()) return {};
    if (ctx.focus && !ctx.focus->empty()) {
        bool roleMatches = ctx.mode == core::LookupMode::Recipes ? role == SlotRole::Output : role != SlotRole::Output;
        if (roleMatches) {
            for (auto const& alt : ing.alternatives) {
                if (alt.matches(*ctx.focus)) {
                    if (focused) *focused = true;
                    return ctx.focus->isWildcard() ? alt : *ctx.focus;
                }
            }
        }
    }
    auto n   = ing.alternatives.size();
    auto idx = static_cast<std::size_t>(std::max(0.0, ctx.time)) % n;
    return ing.alternatives[idx];
}

void ingredientSlot(
    DrawContext&                ctx,
    core::IngredientView const& ing,
    float                       x,
    float                       y,
    SlotRole                    role,
    std::string const&          recipeId,
    bool                        big,
    bool                        missing
) {
    auto& p    = ctx.painter;
    float size = big ? BIG : SLOT;
    p.slot(x, y, size);
    core::Rect r{x, y, size, size};
    core::Rect inner{x + 1, y + 1, size - 2, size - 2};
    bool       focused = false;
    auto       item    = displayed(ing, role, ctx, &focused);
    float      ix = x + (size - 16) / 2, iy = y + (size - 16) / 2;
    if (focused) p.fillRect(inner, palette::HIGHLIGHT);
    if (!item.empty()) {
        p.item(item, ix, iy);
        p.count(ing.count, ix, iy);
    }
    if (missing) p.fillRect(inner, palette::MISSING, Painter::Layer::Foreground);
    if (r.contains(ctx.mouseX, ctx.mouseY)) p.fillRect(inner, palette::HOVER, Painter::Layer::Foreground);
    if (ctx.slots && !item.empty()) {
        ctx.slots->push_back({r, item, static_cast<int>(ing.alternatives.size()), role, recipeId});
    }
}

void drawArrowRight(Painter& p, float x, float y) {
    p.fillRect({x, y + 5, 15, 5}, ARROW_COLOR);
    for (int i = 0; i < 7; ++i) {
        auto f = static_cast<float>(i);
        p.fillRect({x + 15 + f, y + f, 1, 15 - 2 * f}, ARROW_COLOR);
    }
}

void drawArrowDown(Painter& p, float x, float y) {
    p.fillRect({x + 3, y, 5, 6}, ARROW_COLOR);
    for (int i = 0; i < 6; ++i) {
        auto f = static_cast<float>(i);
        p.fillRect({x + f, y + 6 + f, 11 - 2 * f, 1}, ARROW_COLOR);
    }
}

void drawArrowLeftSmall(Painter& p, float x, float y, bool hover) {
    Color c = hover ? Color::rgb(0xFFFFA0) : palette::TEXT_LIGHT;
    for (int i = 0; i < 5; ++i) {
        auto f = static_cast<float>(i);
        p.fillRect({x + 2 + f, y + 4 - f, 1, 1 + 2 * f}, c, Painter::Layer::Foreground);
    }
}

void drawArrowRightSmall(Painter& p, float x, float y, bool hover) {
    Color c = hover ? Color::rgb(0xFFFFA0) : palette::TEXT_LIGHT;
    for (int i = 0; i < 5; ++i) {
        auto f = static_cast<float>(i);
        p.fillRect({x + 6 - f, y + 4 - f, 1, 1 + 2 * f}, c, Painter::Layer::Foreground);
    }
}

void drawFlame(Painter& p, float x, float y) {
    // Row widths from the tip down; the inner (yellow) core is narrower.
    static constexpr int outer[14] = {1, 2, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 12, 10};
    constexpr Color      orange    = Color::rgb(0xE8701A);
    constexpr Color      yellow    = Color::rgb(0xFFD23F);
    for (int row = 0; row < 14; ++row) {
        auto  w  = static_cast<float>(outer[row]);
        float cx = x + 7;
        p.fillRect({std::floor(cx - w / 2), y + static_cast<float>(row), w, 1}, orange);
        if (row >= 6) {
            float iw = w - 4;
            if (iw > 0) p.fillRect({std::floor(cx - iw / 2), y + static_cast<float>(row), iw, 1}, yellow);
        }
    }
}

void drawShapeless(Painter& p, float x, float y) {
    // Two crossing arrows (right on top, left at the bottom), like JEI's shapeless icon.
    p.fillRect({x, y + 2, 6, 1}, ICON_COLOR);
    p.fillRect({x + 6, y + 1, 1, 3}, ICON_COLOR);
    p.fillRect({x + 7, y + 2, 1, 1}, ICON_COLOR);
    p.fillRect({x + 3, y + 6, 6, 1}, ICON_COLOR);
    p.fillRect({x + 2, y + 5, 1, 3}, ICON_COLOR);
    p.fillRect({x + 1, y + 6, 1, 1}, ICON_COLOR);
}

} // namespace bei::ui::recipe
