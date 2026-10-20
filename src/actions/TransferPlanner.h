#pragma once

// Game-independent planning for recipe transfer: where each recipe input goes in the crafting
// grid, which concrete inventory item feeds it, and how many sets fit. No mc/ll includes.

#include "core/model/ItemKey.h"
#include "core/model/RecipeView.h"

#include <string_view>
#include <vector>

namespace bei::actions {

/// The vanilla screen a recipe category is transferred into.
enum class Station {
    None,          ///< Not transferable (campfire, brewing, fuel...).
    Crafting,      ///< Inventory 2x2 grid or crafting table.
    Furnace,       ///< Furnace screen.
    BlastFurnace,  ///< Blast furnace screen.
    Smoker,        ///< Smoker screen.
    SmithingTable, ///< Smithing table screen (template, base, addition).
};

/// Station for a RecipeView::category (core::category ids).
[[nodiscard]] Station stationFor(std::string_view category) noexcept;

/// Game recipe tag of a furnace-like station ("furnace", "blast_furnace", "smoker"), "" otherwise.
/// FurnaceScreenController::mRecipeTag holds the same tag.
[[nodiscard]] std::string_view furnaceTagOf(Station station) noexcept;

/// Inverse of furnaceTagOf (Station::None for unknown tags).
[[nodiscard]] Station furnaceStationOf(std::string_view tag) noexcept;

/// One concrete item kind available to the player (inventory plus crafting grid).
struct PoolEntry {
    core::ItemKey key;           ///< Concrete key (never a wildcard).
    int           count    = 0;  ///< Total count across all slots.
    int           maxStack = 64; ///< Max stack size of the item.
};

/// Grid cell (row-major index in a gridSize x gridSize grid) for every recipe input, or -1
/// for empty inputs. `fits` is false when the recipe does not fit the grid.
struct GridMapping {
    bool             fits = false;
    std::vector<int> cellOfInput;
};

/// Shaped recipes keep their shape (trimmed to the bounding box of non-empty inputs, placed in
/// the top-left corner); shapeless recipes fill the grid row by row.
[[nodiscard]] GridMapping mapToGrid(core::RecipeView const& recipe, int gridSize);

/// Result of matching recipe inputs against a pool.
struct Assignment {
    std::vector<int>           missing; ///< Input indices that cannot be satisfied.
    std::vector<core::ItemKey> chosen;  ///< Concrete item per input (empty key for empty/missing inputs).
    [[nodiscard]] bool         complete() const noexcept { return missing.empty(); }
};

/// Assign a concrete pool item to every non-empty input so that `sets` crafts can be placed.
/// Each grid cell holds a single item kind, so an input needs `count * sets` of one kind.
/// Counts are shared: an item used by one input is not available to another. Inputs with fewer
/// alternatives are matched first, and each input takes the matching kind with the most left.
[[nodiscard]] Assignment assign(core::RecipeView const& recipe, std::vector<PoolEntry> const& pool, int sets);

/// Largest number of sets (1..cap) that can be fully assigned, also bounded by the max stack size
/// of the chosen items. 0 if not even one set is possible.
[[nodiscard]] int maxSets(core::RecipeView const& recipe, std::vector<PoolEntry> const& pool, int cap = 64);

/// Fuel to put into an empty furnace fuel slot: the first of `fuels` (preference order) that the
/// pool holds, skipping the concrete `ingredient` (never burn the item being smelted, e.g. logs
/// for charcoal). Wildcard fuel keys match any aux. Returns the concrete pool key, or an empty
/// key when the player has no usable fuel.
[[nodiscard]] core::ItemKey
pickFuel(std::vector<core::ItemKey> const& fuels, std::vector<PoolEntry> const& pool, core::ItemKey const& ingredient);

/// Add `count` of concrete `key` to the pool (merging with an existing entry).
void addToPool(std::vector<PoolEntry>& pool, core::ItemKey const& key, int count, int maxStack);

} // namespace bei::actions
