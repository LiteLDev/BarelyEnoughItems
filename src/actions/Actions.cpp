// Scaffold stub: nothing is possible yet. Unit "Actions" implements transfer and cheat mode.
#include "actions/Actions.h"

namespace bei::actions {

Actions& Actions::get() {
    static Actions instance;
    return instance;
}

void Actions::install() {}
void Actions::uninstall() {}

TransferCheck Actions::checkTransfer(core::RecipeView const&) { return {}; }

bool Actions::transfer(core::RecipeView const&, bool) { return false; }

bool Actions::canCheat() const { return false; }

bool Actions::give(core::ItemKey const&, int) { return false; }

} // namespace bei::actions
