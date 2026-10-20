#pragma once

#include "core/model/ItemKey.h"
#include "core/state/Bookmarks.h"
#include "core/state/Config.h"
#include "core/state/NavHistory.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace bei::app {

/// Central place for user-level actions, shared state and persistence. UI layers, input and
/// actions talk to each other through this class instead of directly.
/// All methods must be called on the client thread.
class Controller {
public:
    static Controller& get();

    /// Load config/bookmarks from `configDir`. Called from BEI::load.
    void load(std::filesystem::path configDir);
    /// Persist config and bookmarks (cheap; called after every change except search typing).
    void save() const;

    [[nodiscard]] core::Config&    config() noexcept { return mConfig; }
    [[nodiscard]] core::Bookmarks& bookmarks() noexcept { return mBookmarks; }
    [[nodiscard]] core::NavHistory& history() noexcept { return mHistory; }

    // --- Recipe navigation -------------------------------------------------------------
    /// Open the recipe screen for recipes producing `item`. Returns false if there are none.
    bool showRecipes(core::ItemKey const& item);
    /// Open the recipe screen for recipes using `item` (and categories it is a workstation of).
    bool showUses(core::ItemKey const& item);
    /// Open the recipe screen on a whole category.
    bool showCategory(std::string const& category);
    /// Go back one step; closes the recipe screen when history is exhausted.
    void back();
    /// Close the recipe screen and clear history.
    void closeRecipes();

    // --- Hover ---------------------------------------------------------------------------
    /// The item under the cursor: BEI layers first, then the hovered vanilla slot.
    [[nodiscard]] std::optional<core::ItemKey> hoveredItem() const;
    /// Set by input::InputRouter from the vanilla slot hover hooks.
    void setVanillaHovered(std::optional<core::ItemKey> item) { mVanillaHovered = std::move(item); }

    // --- Misc ----------------------------------------------------------------------------
    void toggleBookmark(core::ItemKey const& item);
    void toggleOverlay();

    /// Current item-panel search text (owned here so the recipe screen/search box share it).
    [[nodiscard]] std::string const& searchText() const noexcept { return mConfig.lastSearch; }
    /// Changes are persisted by tick(), debounced (see SEARCH_SAVE_DELAY).
    void                             setSearchText(std::string text);

    /// Search text changes are saved this long after the last keystroke, or as soon as nothing
    /// takes text input any more (search box unfocused, screen closed).
    static constexpr std::chrono::milliseconds SEARCH_SAVE_DELAY{1000};
    /// Called once per UI frame by ui::UiRoot. `typing`: a BEI layer currently takes text input.
    /// Writes a pending debounced save when it is due.
    void tick(bool typing);

private:
    bool show(core::RecipeFocus focus);

    std::filesystem::path        mConfigDir;
    core::Config                 mConfig;
    core::Bookmarks              mBookmarks;
    core::NavHistory             mHistory;
    std::optional<core::ItemKey> mVanillaHovered;
    mutable bool                 mSavePending = false; ///< Unsaved search text (cleared by save()).
    std::chrono::steady_clock::time_point mLastSearchAt; ///< Time of the last search text change.
};

} // namespace bei::app
