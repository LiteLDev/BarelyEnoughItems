// Shared helpers for the non-crafting-table recipe providers (smelting, stonecutter, smithing,
// cartography, brewing, fuel, and the generic fallback).
#pragma once

#include "core/model/RecipeView.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class Item;
class ItemDescriptor;
class ItemStackBase;
class Level;
class Recipe;
class RecipeIngredient;
class Recipes;

namespace bei::game::other {

/// Game recipe tags handled by a dedicated provider in this directory. The fallback provider
/// turns every other tag (except the ones in `kIgnoredTags`) into a generic category.
inline constexpr std::array<std::string_view, 8> kHandledTags{
    "furnace",
    "blast_furnace",
    "smoker",
    "campfire",
    "soul_campfire",
    "stonecutter",
    "smithing_table",
    "cartography_table",
};

/// Tags owned elsewhere (crafting provider) or intentionally hidden.
inline constexpr std::array<std::string_view, 2> kIgnoredTags{"crafting_table", "deprecated"};

[[nodiscard]] bool isHandledOrIgnoredTag(std::string_view tag) noexcept;

/// Key for an item + aux value (-1 / 0x7FFF map to the wildcard). Empty key for invalid items.
[[nodiscard]] core::ItemKey keyOf(Item const& item, int aux);
[[nodiscard]] core::ItemKey keyOf(ItemStackBase const& stack);

/// All concrete items accepted by a descriptor (tags expand to every tagged item).
[[nodiscard]] std::vector<core::ItemKey> expand(ItemDescriptor const& descriptor);
/// Ingredient slot from a recipe ingredient (count = stack size, at least 1).
[[nodiscard]] core::IngredientView toView(RecipeIngredient const& ingredient);
/// Ingredient slot holding exactly one item.
[[nodiscard]] core::IngredientView single(core::ItemKey key, int count = 1);
/// One output slot per result stack. Loads network-delivered results first if needed.
[[nodiscard]] std::vector<core::IngredientView> resultsOf(Level& level, Recipe const& recipe);

/// Stable recipe id ("<game recipe id>", or a synthetic one when the game gives none).
[[nodiscard]] std::string recipeIdOf(Recipe const& recipe, std::string_view fallbackPrefix, std::size_t ordinal);
/// Network id of a recipe as a string ("" when unknown).
[[nodiscard]] std::string netIdOf(Recipe const& recipe);

/// Calls `fn` for each recipe registered under `tag` (no-op when the tag is absent).
/// Exceptions thrown by `fn` for one recipe are logged and skipped.
void forEachRecipeWithTag(
    Recipes&                                                      recipes,
    std::string_view                                              tag,
    std::function<void(std::string const&, Recipe const&)> const& fn
);

/// Every registered item, in registry order. Used for fuel and brewing-reagent discovery.
void forEachRegisteredItem(Level& level, std::function<void(Item const&)> const& fn);

/// A fuel and how long it burns in a regular furnace, in ticks.
struct FuelInfo {
    core::ItemKey key;
    int           burnTicks = 0;
};

/// All fuels in the registry, sorted by burn time (descending) then id.
[[nodiscard]] std::vector<FuelInfo> collectFuels(Level& level);

/// Fuel keys only (coal and charcoal first), for use as Smelting catalysts.
[[nodiscard]] std::vector<core::ItemKey> fuelKeys(std::vector<FuelInfo> const& fuels);

} // namespace bei::game::other
