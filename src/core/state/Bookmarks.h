#pragma once

#include "core/model/ItemKey.h"

#include <filesystem>
#include <vector>

namespace bei::core {

/// Ordered, de-duplicated list of bookmarked items. Newest bookmarks go to the front (JEI default).
class Bookmarks {
public:
    /// Returns true if the item is bookmarked after the call.
    bool toggle(ItemKey const& item);
    void add(ItemKey const& item);
    void remove(ItemKey const& item);
    void clear() noexcept { mItems.clear(); }

    [[nodiscard]] bool                        contains(ItemKey const& item) const;
    [[nodiscard]] std::vector<ItemKey> const& items() const noexcept { return mItems; }

    /// JSON file: {"version":1,"items":["minecraft:stone", ...]}. Missing/corrupt file -> empty list.
    bool load(std::filesystem::path const& file);
    bool save(std::filesystem::path const& file) const;

private:
    std::vector<ItemKey> mItems;
};

} // namespace bei::core
