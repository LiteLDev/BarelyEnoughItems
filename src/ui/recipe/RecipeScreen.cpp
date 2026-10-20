// Scaffold stub: logs requests only. Unit "Recipe view screen" implements the screen.
#include "ui/recipe/RecipeScreen.h"

#include "mod/Log.h"

namespace bei::ui {

RecipeScreen& RecipeScreen::get() {
    static RecipeScreen instance;
    return instance;
}

void RecipeScreen::install() {}
void RecipeScreen::uninstall() {}

void RecipeScreen::show(core::RecipeFocus const& focus) {
    log().info(
        "RecipeScreen::show item={} mode={} category={}",
        focus.item.toString(),
        focus.mode == core::LookupMode::Recipes ? "recipes" : "uses",
        focus.category
    );
}

void RecipeScreen::close() {}

bool RecipeScreen::isOpen() const { return false; }

} // namespace bei::ui
