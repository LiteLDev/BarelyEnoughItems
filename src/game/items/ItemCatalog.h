#pragma once

#include "core/model/ItemEntry.h"
#include "core/model/ItemKey.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class ItemStack;
class ItemStackBase;

namespace bei::game {

/// The list of items shown in the item panel, built from the client's creative inventory
/// (plus registry items missing from it), with names for search.
///
/// Threading: everything runs on the client (render/tick) thread.
/// Lifetime: rebuilt on level join and when the creative content changes; cleared on exit.
class ItemCatalog {
public:
    static ItemCatalog& get();

    /// Install listeners (level join/exit, creative content updates). Called once from BEI::enable.
    void install();
    void uninstall();

    /// Rebuild from the current client level. No-op (and clears) when not in a level.
    void rebuild();
    void clear();

    [[nodiscard]] std::vector<core::ItemEntry> const& entries() const noexcept { return mEntries; }
    /// Incremented by every rebuild()/clear(); consumers cache derived data by generation.
    [[nodiscard]] std::uint64_t                       generation() const noexcept { return mGeneration; }
    [[nodiscard]] core::ItemEntry const*              find(core::ItemKey const& key) const;

    /// Cached display stack for a key (count 1). Wildcard keys resolve to aux 0.
    /// Returns nullptr for unknown items. Valid until the next rebuild()/clear().
    [[nodiscard]] ItemStack const* displayStack(core::ItemKey const& key);

    /// Localized display name for any key (falls back to the id).
    [[nodiscard]] std::string displayName(core::ItemKey const& key);

    /// Build a fresh stack (caller-owned). nullptr for unknown items.
    [[nodiscard]] static std::unique_ptr<ItemStack> makeStack(core::ItemKey const& key, int count = 1);
    /// Key for a game stack (empty key for null stacks).
    [[nodiscard]] static core::ItemKey              keyOf(ItemStackBase const& stack);

private:
    std::vector<core::ItemEntry>                                               mEntries;
    std::unordered_map<core::ItemKey, std::size_t, core::ItemKeyHash>          mIndex;
    std::unordered_map<core::ItemKey, std::unique_ptr<ItemStack>, core::ItemKeyHash> mStacks;
    std::uint64_t                                                              mGeneration = 0;
};

} // namespace bei::game
