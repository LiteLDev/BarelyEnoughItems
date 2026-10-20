#pragma once

#include "core/model/ItemKey.h"

#include <string>
#include <vector>

namespace bei::core {

/// One entry of the item list panel, produced by game::ItemCatalog and consumed by search/UI.
struct ItemEntry {
    ItemKey                  key;
    std::string              displayName; ///< Localized name in the player's current language.
    std::string              englishName; ///< en_US name, for language-independent search.
    std::vector<std::string> tags;        ///< Item tags without '#', e.g. "minecraft:planks".
    std::string              groupName;   ///< Creative group (localized), may be empty.
    int                      order = 0;   ///< Stable ordering (creative inventory order first).
};

} // namespace bei::core
