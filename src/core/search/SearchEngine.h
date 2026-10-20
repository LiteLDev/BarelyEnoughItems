#pragma once

#include "core/model/ItemEntry.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace bei::core {

/// Filters the item list by a JEI-style query.
///
/// Query syntax (target behaviour; the scaffold only implements plain substring terms):
/// - `foo bar`    all terms must match (AND); a term matches display name, English name or id path.
/// - `a|b`        either alternative (OR), binds tighter than space.
/// - `-foo`       negation.
/// - `@ns`        namespace prefix match (e.g. `@minecraft`).
/// - `#tag`       item tag substring (e.g. `#planks`).
/// - `$id`        id substring (e.g. `$oak_`).
/// - `"two words"` quoted literal.
/// Matching is case-insensitive; Chinese names also match by pinyin (full and initials).
class SearchEngine {
public:
    /// Replace the item list. `items` must outlive the engine or until the next setItems call.
    void setItems(std::vector<ItemEntry> const* items);

    /// Indices into the item list, in item order. Empty/blank query returns all items.
    [[nodiscard]] std::vector<std::size_t> filter(std::string_view query) const;

    /// True if a single entry matches (used by tests and the bookmark panel).
    [[nodiscard]] bool matches(ItemEntry const& entry, std::string_view query) const;

private:
    std::vector<ItemEntry> const* mItems = nullptr;
};

} // namespace bei::core
