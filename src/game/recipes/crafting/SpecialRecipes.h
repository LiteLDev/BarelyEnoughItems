#pragma once

#include "core/model/RecipeView.h"

#include <string>
#include <string_view>
#include <vector>

class Recipe;

namespace bei::game::crafting {

class ItemLookup;

/// Concrete kind of a crafting-table recipe, detected from its game vtable.
enum class RecipeKind {
    Shaped,
    Shapeless,
    ShapedChemistry,
    UserDataShapeless,
    // MultiRecipe subclasses (hard-coded "special" recipes whose result depends on the inputs).
    MapCloning,
    MapExtending,
    MapUpgrading,
    MapLocking,
    BookCloning,
    BannerDuplicate,
    BannerAddPattern,
    Fireworks,
    DecoratedPot,
    Shield,
    RepairItem,
    UnknownMulti,
    UnknownShaped,    ///< Unrecognized subclass, treated as shaped.
    UnknownShapeless, ///< Unrecognized subclass, treated as shapeless.
};

[[nodiscard]] RecipeKind       classify(Recipe const& recipe);
[[nodiscard]] std::string_view kindName(RecipeKind kind);
[[nodiscard]] bool             isSpecial(RecipeKind kind);

/// Builds illustrative example recipes (with an explanatory `note` lang key) for a special recipe kind.
/// Appends nothing when the kind has no meaningful fixed display (e.g. item repair) or when the items it
/// needs do not exist in this world.
void buildSpecialExamples(RecipeKind kind, ItemLookup const& items, std::vector<core::RecipeView>& out);

} // namespace bei::game::crafting
