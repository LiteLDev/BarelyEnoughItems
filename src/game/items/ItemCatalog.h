#pragma once

#include "core/model/ItemEntry.h"
#include "core/model/ItemKey.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
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

    ItemCatalog() = default;
    ~ItemCatalog();
    ItemCatalog(ItemCatalog const&)            = delete;
    ItemCatalog& operator=(ItemCatalog const&) = delete;

    /// Install listeners (level join/exit, creative content updates). Called once from BEI::enable.
    void install();
    void uninstall();

    /// Rebuild from the current client level. No-op (and clears) when not in a level.
    void rebuild();
    void clear();

    /// Schedule a rebuild on the client thread. Coalesced; safe to call from any thread or hook.
    /// Used for creative content packets and language changes.
    void requestRebuild();

    [[nodiscard]] std::vector<core::ItemEntry> const& entries() const noexcept { return mEntries; }
    /// Incremented by every rebuild()/clear(); consumers cache derived data by generation.
    [[nodiscard]] std::uint64_t          generation() const noexcept { return mGeneration; }
    [[nodiscard]] core::ItemEntry const* find(core::ItemKey const& key) const;
    /// First (lowest-order) entry with this item id and any aux; nullptr if the id is not listed.
    [[nodiscard]] core::ItemEntry const* findByNameId(std::string_view id) const;
    /// Game language code the names were built for (e.g. "en_US"); empty when not built.
    [[nodiscard]] std::string const& languageCode() const noexcept { return mLanguage; }

    /// Cached display stack for a key (count 1). Wildcard keys resolve to the first listed variant
    /// (aux 0 if the id is not listed). Creative entries keep their creative NBT/block state.
    /// Returns nullptr for unknown items. Valid until the next rebuild()/clear().
    [[nodiscard]] ItemStack const* displayStack(core::ItemKey const& key);

    /// Localized display name for any key (falls back to the id).
    [[nodiscard]] std::string displayName(core::ItemKey const& key);

    /// Build a fresh stack (caller-owned). nullptr for unknown items. Wildcard keys and aux
    /// values the item does not accept fall back to aux 0; count is clamped to 1..255.
    [[nodiscard]] static std::unique_ptr<ItemStack> makeStack(core::ItemKey const& key, int count = 1);
    /// Key for a game stack (empty key for null stacks).
    [[nodiscard]] static core::ItemKey keyOf(ItemStackBase const& stack);

private:
    struct IdHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };

    std::vector<core::ItemEntry>                                                     mEntries;
    std::unordered_map<core::ItemKey, std::size_t, core::ItemKeyHash>                mIndex;
    std::unordered_map<std::string, std::size_t, IdHash, std::equal_to<>>            mFirstById;
    std::unordered_map<core::ItemKey, std::unique_ptr<ItemStack>, core::ItemKeyHash> mStacks;
    std::string                                                                      mLanguage;
    std::uint64_t                                                                    mGeneration = 0;
    std::atomic<bool>                                                                mRebuildQueued{false};
};

} // namespace bei::game
