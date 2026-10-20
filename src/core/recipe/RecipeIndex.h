#pragma once

#include "core/model/ItemKey.h"
#include "core/model/RecipeView.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
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
///   plus *all* recipes of categories that list item as a workstation. Those workstation
///   categories are shown first (like JEI), then the remaining categories; a category
///   never appears twice and a recipe never appears twice within a group.
/// - Matching uses ItemKey::matches (aux wildcard on either side).
/// - Groups are ordered by CategoryInfo::order, then category id; categories referenced by
///   recipes but not registered come after all registered ones, by id. Recipes keep insertion order.
/// - Duplicate recipes (same category + non-empty id) are dropped at build time, first one wins.
/// - Duplicate categories (same id) are merged at build time: the first one (after sorting) wins,
///   and the workstations of the others are appended to it.
///
/// Internally the index keeps inverted posting lists per item id (exact aux buckets plus a
/// wildcard bucket), so lookups cost O(matches) and has() costs O(1) amortised.
class RecipeIndex {
public:
    void build(std::vector<CategoryInfo> categories, std::vector<RecipeView> recipes);
    void clear();

    [[nodiscard]] std::vector<CategoryGroup> lookup(ItemKey const& item, LookupMode mode) const;
    /// All recipes of one category (used when clicking a category's workstation tab).
    [[nodiscard]] std::vector<std::size_t> recipesInCategory(std::string const& category) const;
    /// True if lookup(item, mode) would be non-empty (used to grey out items/hide hints).
    [[nodiscard]] bool has(ItemKey const& item, LookupMode mode) const;

    [[nodiscard]] RecipeView const&                recipe(std::size_t index) const { return mRecipes[index]; }
    [[nodiscard]] std::size_t                      size() const noexcept { return mRecipes.size(); }
    [[nodiscard]] std::vector<CategoryInfo> const& categories() const noexcept { return mCategories; }
    [[nodiscard]] CategoryInfo const*              findCategory(std::string const& id) const;

    /// Total number of recipes lookup(item, mode) would return, without building the groups.
    [[nodiscard]] std::size_t lookupCount(ItemKey const& item, LookupMode mode) const;
    /// Number of recipes in one category (0 for unknown categories). O(1).
    [[nodiscard]] std::size_t categorySize(std::string const& category) const;

private:
    using Pos = std::uint32_t; // position in display order (see mOrder)

    /// Posting lists for one item id. All lists are sorted ascending and duplicate-free.
    struct Postings {
        std::vector<std::pair<std::int16_t, std::vector<Pos>>> exact;    // sorted by aux
        std::vector<Pos>                                       wildcard; // alternatives with ANY_AUX
        std::vector<Pos>                                       any;      // union of everything above
    };
    struct Station {
        ItemKey       key;
        std::uint32_t group;
    };

    /// Calls fn(pos) for every recipe position whose inputs/outputs match item, ascending, once each.
    template <class Fn>
    void forEachMatch(ItemKey const& item, LookupMode mode, Fn&& fn) const;
    /// Ranks of non-empty categories that list item as a workstation, ascending.
    [[nodiscard]] std::vector<std::uint32_t> stationGroups(ItemKey const& item) const;

    std::vector<CategoryInfo> mCategories; // sorted by order, id
    std::vector<RecipeView>   mRecipes;    // insertion order (after dedup)

    // Display order: recipes sorted by (group rank, insertion index).
    std::vector<std::uint32_t> mOrder;    // pos -> recipe index
    std::vector<std::uint32_t> mPosGroup; // pos -> group rank

    // Groups: one per distinct category (registered first, then unregistered by id).
    std::vector<std::string>                              mGroupCategory; // rank -> category id
    std::vector<std::pair<Pos, Pos>>                      mGroupRange;    // rank -> [begin, end) in mOrder
    std::unordered_map<std::string, std::uint32_t>        mGroupRank;     // category id -> rank (== mCategories index
                                                                          // for registered categories)
    std::unordered_map<std::string, Postings>             mOutputs;
    std::unordered_map<std::string, Postings>             mInputs;
    std::unordered_map<std::string, std::vector<Station>> mStations; // workstation id -> categories
};

} // namespace bei::core
