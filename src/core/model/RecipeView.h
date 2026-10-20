#pragma once

#include "core/model/ItemKey.h"

#include <string>
#include <vector>

namespace bei::core {

/// Category identifiers. Providers may also use any other string for runtime-discovered categories.
namespace category {
inline constexpr char const* CRAFTING      = "minecraft:crafting";
inline constexpr char const* FURNACE       = "minecraft:furnace";
inline constexpr char const* BLAST_FURNACE = "minecraft:blast_furnace";
inline constexpr char const* SMOKER        = "minecraft:smoker";
inline constexpr char const* CAMPFIRE      = "minecraft:campfire";
inline constexpr char const* SOUL_CAMPFIRE = "minecraft:soul_campfire";
inline constexpr char const* BREWING       = "minecraft:brewing";
inline constexpr char const* SMITHING      = "minecraft:smithing";
inline constexpr char const* STONECUTTER   = "minecraft:stonecutter";
inline constexpr char const* CARTOGRAPHY   = "minecraft:cartography";
inline constexpr char const* FUEL          = "minecraft:fuel";
} // namespace category

/// How the recipe screen lays out a category.
enum class LayoutKind {
    CraftingGrid, ///< inputs = width*height cells (row-major), one output.
    Smelting,     ///< inputs[0] = input, catalysts = fuels, one output.
    Brewing,      ///< inputs[0] = base potion, inputs[1] = reagent, catalysts = blaze powder, one output.
    Smithing,     ///< inputs = template, base, addition; one output.
    SingleInput,  ///< inputs[0] -> outputs (stonecutter etc).
    Generic,      ///< inputs listed left-to-right, outputs on the right.
};

/// Static description of a recipe category.
struct CategoryInfo {
    std::string          id;           ///< e.g. category::CRAFTING.
    std::string          titleKey;     ///< Lang key for the title, e.g. "bei.category.crafting".
    std::string          fallbackTitle; ///< Used when titleKey has no translation.
    LayoutKind           layout = LayoutKind::Generic;
    std::vector<ItemKey> workstations; ///< Blocks that perform this category (crafting table...).
    int                  order  = 1000; ///< Tab ordering, lower first.
};

/// One slot of a recipe: any of `alternatives` is accepted (cycled in the UI).
struct IngredientView {
    std::vector<ItemKey> alternatives; ///< Empty means "empty slot".
    int                  count = 1;

    [[nodiscard]] bool empty() const noexcept { return alternatives.empty(); }
};

/// Game-independent view of a recipe, produced by game::IRecipeProvider.
struct RecipeView {
    std::string                 id;       ///< Unique per category (game recipe id or synthetic id).
    std::string                 category; ///< CategoryInfo::id.
    int                         width  = 0; ///< Grid width (CraftingGrid only, 1..3).
    int                         height = 0; ///< Grid height (CraftingGrid only, 1..3).
    bool                        shapeless = false;
    std::vector<IngredientView> inputs;
    std::vector<IngredientView> outputs;
    std::vector<IngredientView> catalysts; ///< Fuel, blaze powder, etc. Not counted as "uses".
    std::string                 note;      ///< Optional extra text (lang key or literal).
    std::string                 netId;     ///< Game recipe net id (as string) if known, for transfer.
};

} // namespace bei::core
