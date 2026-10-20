#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace bei::core {

/// Identifies an item variant independent of the game: full identifier plus aux (data) value.
/// NBT is intentionally not part of the key; BEI treats items with the same id/aux as the same entry.
struct ItemKey {
    /// Aux value that matches any variant of the item (Bedrock uses 0x7FFF as the wildcard).
    static constexpr std::int16_t ANY_AUX = 0x7FFF;

    std::string  id;      ///< Fully qualified identifier, e.g. "minecraft:oak_planks".
    std::int16_t aux = 0; ///< Data value, or ANY_AUX.

    [[nodiscard]] bool empty() const noexcept { return id.empty(); }
    [[nodiscard]] bool isWildcard() const noexcept { return aux == ANY_AUX; }

    /// True if this key (possibly a wildcard) accepts `concrete`.
    [[nodiscard]] bool matches(ItemKey const& concrete) const noexcept {
        return id == concrete.id && (isWildcard() || concrete.isWildcard() || aux == concrete.aux);
    }

    /// Namespace part of the id ("minecraft" for "minecraft:stone"; "minecraft" if no colon).
    [[nodiscard]] std::string_view nameSpace() const noexcept;
    /// Path part of the id ("stone" for "minecraft:stone").
    [[nodiscard]] std::string_view path() const noexcept;

    /// "minecraft:stone" or "minecraft:stone:3" (aux omitted when 0, "*" for wildcard).
    [[nodiscard]] std::string toString() const;
    /// Inverse of toString(). Returns nullopt for malformed input.
    [[nodiscard]] static std::optional<ItemKey> parse(std::string_view text);

    friend bool operator==(ItemKey const&, ItemKey const&)                  = default;
    friend std::strong_ordering operator<=>(ItemKey const&, ItemKey const&) = default;
};

struct ItemKeyHash {
    std::size_t operator()(ItemKey const& k) const noexcept {
        return std::hash<std::string>{}(k.id) * 31u + static_cast<std::uint16_t>(k.aux);
    }
};

} // namespace bei::core
