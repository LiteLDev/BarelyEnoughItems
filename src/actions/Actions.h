#pragma once

#include "core/model/ItemKey.h"
#include "core/model/RecipeView.h"

#include <vector>

namespace bei::actions {

/// Result of checking whether a recipe can be moved into the open crafting grid.
struct TransferCheck {
    bool             possible = false; ///< A compatible crafting screen is open and the recipe fits.
    std::vector<int> missingInputs;    ///< Indices into RecipeView::inputs the player lacks.
    [[nodiscard]] bool canCraft() const noexcept { return possible && missingInputs.empty(); }
};

/// Player-affecting actions: recipe transfer ("+" button) and cheat mode.
///
/// Contract:
/// - All calls on the client thread; never touch inventories from input callbacks directly.
/// - checkTransfer/transfer only support crafting-table recipes in the crafting table screen
///   and recipes fitting 2x2 in the survival inventory screen; everything else is not possible.
/// - transfer(maxSets=true) moves as many sets as available (shift-click in JEI).
/// - give() only works when canCheat() (operator / creative); it uses the /give command.
class Actions {
public:
    static Actions& get();

    void install();
    void uninstall();

    [[nodiscard]] TransferCheck checkTransfer(core::RecipeView const& recipe);
    bool                        transfer(core::RecipeView const& recipe, bool maxSets);

    [[nodiscard]] bool canCheat() const;
    bool               give(core::ItemKey const& item, int count);
};

} // namespace bei::actions
