#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bei::core {

/// User configuration, persisted as JSON in the mod's config dir (config/config.json).
/// Key bindings are owned by LeviLamina's KeyRegistry and are not stored here.
struct Config {
    /// Version written by this build. Older files are migrated on load (see Config.cpp `migrate`).
    static constexpr int         CURRENT_VERSION   = 1;
    static constexpr int         MAX_COLUMNS       = 64;  ///< Upper bound for maxColumns.
    static constexpr int         MAX_STACK_SIZE    = 64;  ///< Upper bound for cheatGiveStackSize.
    static constexpr std::size_t MAX_SEARCH_LENGTH = 256; ///< lastSearch is truncated to this many bytes.
    static constexpr std::size_t MAX_SEARCH_HISTORY = 20; ///< searchHistory keeps this many entries.

    int  version             = CURRENT_VERSION; ///< After load: max(file version, CURRENT_VERSION).
    bool overlayEnabled      = true;            ///< Item list panel visible (toggled with O).
    bool bookmarksEnabled    = true;            ///< Bookmark panel visible.
    bool cheatMode           = false;           ///< Clicking an item gives it (requires permission).
    bool showOnAllContainers = true;  ///< Show the overlay on every container screen, not just inventory/crafting.
    bool centerSearchBar     = false; ///< Put the search bar under the container instead of in the panel.
    int  maxColumns          = 0;     ///< Item panel columns; 0 = as many as fit. Clamped to [0, MAX_COLUMNS].
    std::string lastSearch;           ///< Restored when the overlay opens.

    // Added after the scaffold (all optional in the file; defaults apply when missing).
    int  cheatGiveStackSize  = 64;    ///< Count given by a cheat-mode left click. Clamped to [1, MAX_STACK_SIZE].
    bool showItemIds         = false; ///< Advanced tooltips: show "minecraft:stone" (and aux) in item tooltips.
    bool showModName         = true;  ///< Show the item's namespace (e.g. "Minecraft") as the last tooltip line.
    bool bookmarksAddToFront = true;  ///< New bookmarks go to the front (JEI default) instead of the back.
    bool rememberPage        = false; ///< Restore lastPage when the overlay opens.
    int  lastPage            = 0;     ///< Item list page last shown (>= 0).
    /// Submitted searches, newest first: distinct, non-blank, each at most MAX_SEARCH_LENGTH bytes,
    /// at most MAX_SEARCH_HISTORY entries (Up/Down in the search box cycle through them).
    std::vector<std::string> searchHistory;

    /// Fields found in the loaded file that this build doesn't know (a JSON object as text, or
    /// empty). Written back unchanged by save() so a newer BEI's settings survive a downgrade.
    std::string extraFields;

    /// Missing/corrupt file -> defaults (and returns false). Unknown fields are ignored, fields
    /// with the wrong type or out of range keep their defaults / are clamped.
    bool load(std::filesystem::path const& file);
    /// Atomic (temp file + rename). Returns false on I/O failure; the old file is then untouched.
    bool save(std::filesystem::path const& file) const;

    /// Same as load/save, on JSON text.
    bool                      fromJsonText(std::string_view text);
    [[nodiscard]] std::string toJsonText() const;

    /// Clamp numeric fields into their valid ranges (and normalize searchHistory).
    void sanitize();

    /// Record a submitted search: moved to the front of searchHistory (duplicates removed, the
    /// oldest entry dropped past MAX_SEARCH_HISTORY). Blank text is ignored. Returns true if
    /// searchHistory changed.
    bool pushSearchHistory(std::string_view text);
};

} // namespace bei::core
