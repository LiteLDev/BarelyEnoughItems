#pragma once

#include "core/model/ItemKey.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bei::core {

/// Ordered, de-duplicated list of bookmarked items. Newest bookmarks go to the front (JEI default).
class Bookmarks {
public:
    /// Default upper bound on the number of bookmarks (adds past it are refused).
    static constexpr std::size_t MAX_COUNT = 1024;

    /// Returns true if the item is bookmarked after the call.
    bool toggle(ItemKey const& item);
    /// Add to the front. No-op for empty keys, duplicates, or when full.
    void add(ItemKey const& item);
    /// Insert at `index` (clamped to [0, size]). Returns false if empty/duplicate/full.
    bool insert(std::size_t index, ItemKey const& item);
    void remove(ItemKey const& item);
    void clear() noexcept { mItems.clear(); }

    /// Move an existing bookmark to `index` (clamped to [0, size-1]); the other entries keep their
    /// relative order. Returns false if the item isn't bookmarked.
    bool moveTo(ItemKey const& item, std::size_t index);
    /// Move the entry at `from` to `to` (clamped). Returns false if `from` is out of range.
    bool move(std::size_t from, std::size_t to);

    [[nodiscard]] bool                        contains(ItemKey const& item) const;
    [[nodiscard]] std::optional<std::size_t>  indexOf(ItemKey const& item) const;
    [[nodiscard]] std::vector<ItemKey> const& items() const noexcept { return mItems; }
    [[nodiscard]] std::size_t                 size() const noexcept { return mItems.size(); }
    [[nodiscard]] bool                        empty() const noexcept { return mItems.empty(); }
    [[nodiscard]] bool                        full() const noexcept { return mItems.size() >= mMaxCount; }

    /// Change the maximum count (minimum 1). Existing entries past the limit are dropped from the back.
    void                      setMaxCount(std::size_t count);
    [[nodiscard]] std::size_t maxCount() const noexcept { return mMaxCount; }

    /// JSON file: {"version":1,"items":["minecraft:stone", ...]}. Missing/corrupt file -> empty list
    /// and false. Malformed or duplicate entries are skipped; order is kept. A bare JSON array of
    /// strings is accepted as well. Loading never drops valid entries, even past maxCount() (only
    /// further adds are refused), so a lower limit can't silently delete saved bookmarks.
    bool load(std::filesystem::path const& file);
    /// Atomic (temp file + rename). Returns false on I/O failure; the old file is then untouched.
    bool save(std::filesystem::path const& file) const;

    /// Same as load/save, on JSON text.
    bool                      fromJsonText(std::string_view text);
    [[nodiscard]] std::string toJsonText() const;

private:
    std::vector<ItemKey> mItems;
    std::size_t          mMaxCount = MAX_COUNT;
};

} // namespace bei::core
