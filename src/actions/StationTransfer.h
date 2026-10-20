#pragma once

// Internal to src/actions: recipe transfer into the furnace-like screens (furnace, blast furnace,
// smoker) and the smithing table. Actions::checkTransfer/transfer dispatch here for those
// categories; crafting stays in Actions.cpp.

#include "actions/Actions.h"
#include "core/model/RecipeView.h"

namespace bei::actions::station {

void install();
void uninstall();

/// True for the categories this file transfers (smelting family and smithing).
[[nodiscard]] bool handles(core::RecipeView const& recipe);

[[nodiscard]] TransferCheck check(core::RecipeView const& recipe);
bool                        transfer(core::RecipeView const& recipe, bool maxSets);

} // namespace bei::actions::station
