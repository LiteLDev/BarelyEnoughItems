#include "app/Controller.h"

#include "game/recipes/RecipeManager.h"
#include "mod/Log.h"
#include "ui/UiRoot.h"
#include "ui/recipe/RecipeScreen.h"

namespace bei::app {

Controller& Controller::get() {
    static Controller instance;
    return instance;
}

void Controller::load(std::filesystem::path configDir) {
    mConfigDir = std::move(configDir);
    mConfig.load(mConfigDir / "config.json");
    mBookmarks.load(mConfigDir / "bookmarks.json");
    save(); // writes defaults on first run
}

void Controller::save() const {
    mSavePending = false;
    if (mConfigDir.empty()) return;
    mConfig.save(mConfigDir / "config.json");
    mBookmarks.save(mConfigDir / "bookmarks.json");
}

bool Controller::show(core::RecipeFocus focus) {
    auto& index = game::RecipeManager::get().index();
    bool  any   = focus.item.empty() ? index.categorySize(focus.category) > 0 : index.has(focus.item, focus.mode);
    if (!any) return false;
    mHistory.push(focus);
    ui::RecipeScreen::get().show(*mHistory.current());
    return true;
}

bool Controller::showRecipes(core::ItemKey const& item) {
    return show({.item = item, .mode = core::LookupMode::Recipes});
}

bool Controller::showUses(core::ItemKey const& item) { return show({.item = item, .mode = core::LookupMode::Uses}); }

bool Controller::showCategory(std::string const& category) { return show({.category = category}); }

void Controller::back() {
    if (auto prev = mHistory.back()) {
        ui::RecipeScreen::get().show(*prev);
    } else {
        ui::RecipeScreen::get().close();
    }
}

void Controller::closeRecipes() {
    mHistory.clear();
    ui::RecipeScreen::get().close();
}

std::optional<core::ItemKey> Controller::hoveredItem() const {
    auto& frame = ui::UiRoot::get().lastFrame();
    if (auto item = ui::UiRoot::get().itemAt(frame.mouseX, frame.mouseY)) return item;
    if (ui::UiRoot::get().contains(frame.mouseX, frame.mouseY)) return std::nullopt;
    return mVanillaHovered;
}

void Controller::toggleBookmark(core::ItemKey const& item) {
    mBookmarks.toggle(item);
    save();
}

void Controller::toggleOverlay() {
    mConfig.overlayEnabled = !mConfig.overlayEnabled;
    save();
}

void Controller::setSearchText(std::string text) {
    if (text == mConfig.lastSearch) return;
    mConfig.lastSearch = std::move(text);
    // Saved by tick() once typing stops, not on every keystroke.
    mSavePending  = true;
    mLastSearchAt = std::chrono::steady_clock::now();
}

void Controller::tick(bool typing) {
    if (!mSavePending) return;
    if (typing && std::chrono::steady_clock::now() - mLastSearchAt < SEARCH_SAVE_DELAY) return;
    save();
}

} // namespace bei::app
