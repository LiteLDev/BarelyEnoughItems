#pragma once

#include "core/model/ItemKey.h"
#include "core/model/RecipeView.h"

#include <cstddef>
#include <string>
#include <vector>

namespace bei::core {

enum class LookupMode {
    Recipes, ///< Recipes that produce the item ("R" in JEI).
    Uses,    ///< Recipes that consume the item, plus categories whose workstation it is ("U").
};

/// Recipes of one category matching a lookup, in display order.
struct CategoryGroup {
    std::string              category;
    std::vector<std::size_t> recipes; ///< Indices into RecipeIndex::recipe().
};

/// Immutable-after-build index over all recipes and categories.
///
/// Lookup semantics:
/// - Recipes(item): recipes with an output alternative matching item.
/// - Uses(item): recipes with an input alternative matching item (catalysts excluded),
///   plus *all* recipes of categories that list item as a workstation (shown first, like JEI).
/// - Matching uses ItemKey::matches (aux wildcard on either side).
/// - Groups are ordered by CategoryInfo::order, then category id; recipes keep insertion order.
/// - Duplicate recipes (same category + id) are dropped at build time, first one wins.
class RecipeIndex {
public:
    void build(std::vector<CategoryInfo> categories, std::vector<RecipeView> recipes);
    void clear();

    [[nodiscard]] std::vector<CategoryGroup> lookup(ItemKey const& item, LookupMode mode) const;
    /// All recipes of one category (used when clicking a category's workstation tab).
    [[nodiscard]] std::vector<std::size_t>   recipesInCategory(std::string const& category) const;
    /// True if lookup(item, mode) would be non-empty (used to grey out items/hide hints).
    [[nodiscard]] bool                       has(ItemKey const& item, LookupMode mode) const;

    [[nodiscard]] RecipeView const&                recipe(std::size_t index) const { return mRecipes[index]; }
    [[nodiscard]] std::size_t                      size() const noexcept { return mRecipes.size(); }
    [[nodiscard]] std::vector<CategoryInfo> const& categories() const noexcept { return mCategories; }
    [[nodiscard]] CategoryInfo const*              findCategory(std::string const& id) const;

private:
    std::vector<CategoryInfo> mCategories; // sorted by order, id
    std::vector<RecipeView>   mRecipes;
};

} // namespace bei::core
