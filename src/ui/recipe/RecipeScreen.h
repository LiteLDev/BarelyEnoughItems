#pragma once

#include "core/model/ItemKey.h"
#include "core/state/NavHistory.h"

#include <optional>

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
///
/// How it works (see RecipeScreen.cpp):
/// - show() pushes a native dialog scene (SceneFactory::createCommonDialogInfoScreen) so the
///   game gives BEI focus and a cursor from any screen (HUD, inventory, crafting table...).
///   That scene's ScreenView is never drawn natively: BEI renders its own UI in its place
///   through ui::UiRoot, where an IUiLayer ("recipe_screen", zOrder 1000) draws the GUI.
/// - Mouse input reaches the layer through input::InputRouter/UiRoot like any other layer;
///   the layer contains() the whole screen, so vanilla sees no clicks or wheel while open.
/// - While the screen is the top scene it handles ESC, Backspace, Left/Right/PageUp/PageDown
///   (pages) and Up/Down (categories) itself: the ll::event::KeyInputEvent is cancelled
///   (listener priority High). Other BEI key handlers must ignore cancelled key events.
///   R/U/A hotkeys keep working through app::Controller::hoveredItem(), which asks UiRoot
///   (and thus this screen's itemAt()).
/// - The scene being popped by anything (ESC, a dimension change, a disconnect), level exit,
///   or the scene disappearing from the stack (watchdog) closes the screen and clears history.
class RecipeScreen {
public:
    static RecipeScreen& get();

    void install();
    void uninstall();

    void               show(core::RecipeFocus const& focus);
    void               close();
    [[nodiscard]] bool isOpen() const;

    /// Item under the point (GUI units) in the last rendered frame of the screen, if open.
    [[nodiscard]] std::optional<core::ItemKey> itemAt(float x, float y) const;
};

} // namespace bei::ui
