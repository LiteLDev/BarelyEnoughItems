#pragma once

#include "core/model/ItemEntry.h"
#include "core/search/Pinyin.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bei::core {

/// Filters the item list by a JEI-style query.
///
/// Query syntax:
/// - `foo bar`    all terms must match (AND); a term matches display name, English name or id path
///                (or the full id when the term contains ':', so pasted ids like `minecraft:stick` work).
/// - `a|b`        either alternative (OR), binds tighter than space (`a | b` is the same as `a|b`).
/// - `-foo`       negation (per alternative: `-a|b` is "not a, or b").
/// - `@ns`        namespace prefix match (e.g. `@minecraft`, `@mine`).
/// - `#tag`       item tag substring (e.g. `#planks`).
/// - `$id`        full id substring (e.g. `$oak_`, `$minecraft:stick`).
/// - `"two words"` quoted literal (may contain spaces and `|`; combines with prefixes: `-$"oak_"`).
/// Prefixes combine as `-` then one of `@#$`. Alternatives whose text is empty (e.g. a lone `-` or `@`
/// while the user is still typing) are ignored.
///
/// Matching is case-insensitive and diacritic-insensitive on decoded UTF-8 (fullwidth forms fold to
/// ASCII, Minecraft `§` formatting codes are ignored). Chinese display names also match by pinyin:
/// full pinyin (`muban`), initials (`mb`), any mix (`mub`, `mban`) and mixed with Hanzi (`木ban`).
/// Quoted literals match by substring only (no pinyin).
///
/// Caching: per-item normalized/pinyin keys are computed in setItems() (only for entries whose text
/// changed since the previous call). filter() remembers its last query and result; when the new query
/// can only narrow the previous one (e.g. the user typed more characters), only the previous result is
/// rescanned. The engine is not thread-safe (filter() mutates the cache).
class SearchEngine {
public:
    /// Replace the item list. `items` must outlive the engine or until the next setItems call.
    /// Call again after mutating the list; unchanged entries keep their precomputed keys.
    void setItems(std::vector<ItemEntry> const* items);

    /// Same as setItems(items), but skips the per-entry change check when `items` and `revision` are the
    /// same as in the previous call (e.g. pass ItemCatalog::generation()). Use when the caller knows the
    /// list only changes together with its revision.
    void setItems(std::vector<ItemEntry> const* items, std::uint64_t revision);

    /// Indices into the item list, in item order. Empty/blank query returns all items.
    [[nodiscard]] std::vector<std::size_t> filter(std::string_view query) const;

    /// True if a single entry matches (used by tests and the bookmark panel).
    [[nodiscard]] bool matches(ItemEntry const& entry, std::string_view query) const;

    /// Number of entries tested by the last filter() call (diagnostics / tests).
    [[nodiscard]] std::size_t lastScanCount() const { return mLastScanCount; }

    /// Total number of per-entry key computations done by setItems() so far (diagnostics / tests).
    [[nodiscard]] std::size_t keyBuildCount() const { return mKeyBuildCount; }

    // --- Implementation types (public so the .cpp's helpers can use them; not part of the contract) ---

    /// One alternative of a clause, e.g. `-@minecraft`.
    struct Term {
        enum class Kind : std::uint8_t { Text, Namespace, Tag, Id };
        Kind        kind    = Kind::Text;
        bool        negated = false;
        bool        quoted  = false;
        std::string text; ///< Folded.
    };
    /// OR of terms.
    struct Clause {
        std::vector<Term> terms;
    };
    /// AND of clauses.
    struct Query {
        std::vector<Clause> clauses;
    };

    /// One code point of the folded display name, for pinyin matching.
    struct NameUnit {
        std::uint32_t begin        = 0; ///< Offset of the folded text in `Keys::name`.
        std::uint16_t length       = 0; ///< Length of the folded text in bytes.
        std::uint8_t  readingCount = 0;
        std::uint16_t readings[pinyin::kMaxReadings]{}; ///< pinyin syllable ids
    };
    /// Precomputed search keys of one entry.
    struct Keys {
        std::string              name;    ///< Folded display name.
        std::string              english; ///< Folded English name.
        std::string              id;      ///< Folded full id.
        std::string              idPath;  ///< Folded id path (after ':').
        std::string              ns;      ///< Folded namespace (before ':', "minecraft" if absent).
        std::vector<std::string> tags;    ///< Folded tags.
        std::vector<NameUnit>    units;   ///< Only filled when the display name contains ideographs.
        // Source snapshot used to detect changed entries in setItems().
        bool                     built = false;
        std::string              srcName;
        std::string              srcEnglish;
        std::string              srcId;
        std::vector<std::string> srcTags;
    };

    /// Parses a query (exposed for tests).
    [[nodiscard]] static Query parse(std::string_view query);

private:
    std::vector<ItemEntry> const* mItems = nullptr;
    std::vector<Keys>             mKeys;
    std::size_t                   mKeyBuildCount = 0;
    std::uint64_t                 mRevision      = 0;
    bool                          mHasRevision   = false;

    mutable bool                     mHasLast = false;
    mutable Query                    mLastQuery;
    mutable std::vector<std::size_t> mLastResult;
    mutable std::size_t              mLastScanCount = 0;
};

} // namespace bei::core
