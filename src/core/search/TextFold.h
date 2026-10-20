#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace bei::core::text {

inline constexpr char32_t kReplacementChar = 0xFFFD;

/// Decodes the code point starting at `s[pos]` and advances `pos` past it.
/// Invalid, overlong, surrogate or truncated sequences yield U+FFFD and advance by one byte.
/// Precondition: `pos < s.size()`.
[[nodiscard]] char32_t decodeUtf8(std::string_view s, std::size_t& pos);

/// Appends `cp` encoded as UTF-8 (invalid code points are written as U+FFFD).
void appendUtf8(std::string& out, char32_t cp);

/// Appends the search-folded form of a single code point: lowercase, diacritics removed
/// (Latin-1, Latin Extended-A, Greek tonos, Cyrillic yo), fullwidth ASCII mapped to ASCII,
/// ideographic/no-break spaces mapped to ' '. Combining marks append nothing.
void foldCodepoint(char32_t cp, std::string& out);

/// Calls `fn(char32_t original, std::string_view folded)` for every code point of `s`. When
/// `stripFormatting` is set, Minecraft formatting codes (`§` plus the following character) are skipped.
/// `folded` may be empty.
template <class Fn>
void forEachFolded(std::string_view s, Fn&& fn, bool stripFormatting = true) {
    std::string buf;
    std::size_t pos = 0;
    while (pos < s.size()) {
        char32_t cp = decodeUtf8(s, pos);
        if (stripFormatting && cp == U'§') { // formatting code: drop it and its argument
            if (pos < s.size()) (void)decodeUtf8(s, pos);
            continue;
        }
        buf.clear();
        foldCodepoint(cp, buf);
        fn(cp, std::string_view{buf});
    }
}

/// Folds a whole UTF-8 string (see foldCodepoint / forEachFolded). The result is valid UTF-8.
[[nodiscard]] std::string fold(std::string_view s, bool stripFormatting = true);

} // namespace bei::core::text
