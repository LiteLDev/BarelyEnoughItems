#pragma once

#include <filesystem>
#include <string>

namespace bei::core {

/// User configuration, persisted as JSON in the mod's config dir (config/config.json).
/// Key bindings are owned by LeviLamina's KeyRegistry and are not stored here.
struct Config {
    int  version            = 1;
    bool overlayEnabled     = true;  ///< Item list panel visible (toggled with O).
    bool bookmarksEnabled   = true;  ///< Bookmark panel visible.
    bool cheatMode          = false; ///< Clicking an item gives it (requires permission).
    bool showOnAllContainers = true; ///< Show the overlay on every container screen, not just inventory/crafting.
    bool centerSearchBar    = false; ///< Put the search bar under the container instead of in the panel.
    int  maxColumns         = 0;     ///< 0 = as many as fit.
    std::string lastSearch;           ///< Restored when the overlay opens.

    /// Missing/corrupt file -> defaults (and returns false). Unknown fields are ignored.
    bool load(std::filesystem::path const& file);
    bool save(std::filesystem::path const& file) const;
};

} // namespace bei::core
