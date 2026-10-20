#pragma once

#include "core/model/RecipeView.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class ItemDescriptor;
class ItemInstance;
class Level;

namespace bei::game::crafting {

/// Upper bound on the alternatives of one ingredient. Tag descriptors rarely come close; the cap only
/// protects against pathological data-driven descriptors.
inline constexpr std::size_t MAX_ALTERNATIVES = 512;

/// Expands a game ItemDescriptor (plain item, item+aux, wildcard aux, item tag, molang...) into the
/// concrete item keys it accepts. Wildcard aux values are kept as core::ItemKey::ANY_AUX.
/// Returns an empty ingredient for null/invalid descriptors. Never throws on bad data.
[[nodiscard]] core::IngredientView toIngredient(ItemDescriptor const& descriptor, int count);

/// True for descriptors that intentionally match nothing (empty grid cells).
[[nodiscard]] bool isEmptyDescriptor(ItemDescriptor const& descriptor);

/// Converts recipe result instances to output ingredients (null instances are skipped).
[[nodiscard]] std::vector<core::IngredientView> toOutputs(std::vector<ItemInstance> const& results);

/// Marks `view` shapeless and picks a JEI-style grid for its inputs: 1x1 for one input, 2x2 for up to
/// four, 3x3 otherwise. Inputs are padded with empty slots to width*height. Returns false when the
/// inputs do not fit a 3x3 grid (the view is left unchanged).
bool layoutShapeless(core::RecipeView& view);

/// Order-sensitive textual signature of a recipe's displayed content, used for de-duplication.
[[nodiscard]] std::string signatureOf(core::RecipeView const& view);

/// Looks up items by full name in the level's item registry (to drop items a world does not have).
class ItemLookup {
public:
    explicit ItemLookup(Level& level) : mLevel(level) {}

    [[nodiscard]] bool exists(std::string_view fullName) const;
    /// Key for `fullName` if the item exists, else an empty key.
    [[nodiscard]] core::ItemKey key(std::string_view fullName, short aux = 0) const;
    /// Ingredient with every existing item of `fullNames` (aux applied to all).
    [[nodiscard]] core::IngredientView
    anyOf(std::vector<std::string_view> const& fullNames, short aux = 0, int count = 1) const;

private:
    Level& mLevel;
};

} // namespace bei::game::crafting
