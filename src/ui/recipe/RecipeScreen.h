#pragma once

#include "core/state/NavHistory.h"

namespace bei::ui {

/// Full-screen recipe viewer (the "recipes GUI" in JEI).
///
/// Contract:
/// - show(focus) opens the screen (or re-targets it if already open) on the given focus;
///   focus.category/categoryTab/page select the initial tab and page.
/// - The screen writes tab/page changes back into app::Controller::history().current() so
///   Back restores them.
/// - Clicking an ingredient/output calls Controller::showRecipes (left) / showUses (right);
///   Backspace calls Controller::back(); ESC or the close button calls Controller::closeRecipes().
/// - close() closes it without touching history. isOpen() reflects the real game state
///   (the scene may be dropped by the game without notice).
class RecipeScreen {
public:
    static RecipeScreen& get();

    void install();
    void uninstall();

    void               show(core::RecipeFocus const& focus);
    void               close();
    [[nodiscard]] bool isOpen() const;
};

} // namespace bei::ui
