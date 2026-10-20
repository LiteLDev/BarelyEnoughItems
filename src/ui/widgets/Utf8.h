#pragma once

// Small UTF-8 helpers shared by the widgets (caret movement, wrapping, input filtering).
// Header-only and free of game dependencies.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace bei::ui::utf8 {

[[nodiscard]] inline bool isContinuation(char c) noexcept { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

/// Byte offset of the code point boundary after `pos` (clamped to the string size).
[[nodiscard]] inline std::size_t next(std::string_view s, std::size_t pos) noexcept {
    if (pos >= s.size()) return s.size();
    ++pos;
    while (pos < s.size() && isContinuation(s[pos])) ++pos;
    return pos;
}

/// Byte offset of the code point boundary before `pos` (0 at the start).
[[nodiscard]] inline std::size_t prev(std::string_view s, std::size_t pos) noexcept {
    if (pos > s.size()) pos = s.size();
    if (pos == 0) return 0;
    --pos;
    while (pos > 0 && isContinuation(s[pos])) --pos;
    return pos;
}

/// Snap `pos` back onto a code point boundary.
[[nodiscard]] inline std::size_t floorBoundary(std::string_view s, std::size_t pos) noexcept {
    if (pos >= s.size()) return s.size();
    while (pos > 0 && isContinuation(s[pos])) --pos;
    return pos;
}

/// Decode the code point starting at `pos` (U+FFFD for malformed input).
[[nodiscard]] inline char32_t decode(std::string_view s, std::size_t pos) noexcept {
    if (pos >= s.size()) return 0;
    auto        b0 = static_cast<unsigned char>(s[pos]);
    std::size_t n  = next(s, pos) - pos;
    if (b0 < 0x80) return b0;
    char32_t cp;
    if ((b0 & 0xE0) == 0xC0 && n == 2) cp = b0 & 0x1F;
    else if ((b0 & 0xF0) == 0xE0 && n == 3) cp = b0 & 0x0F;
    else if ((b0 & 0xF8) == 0xF0 && n == 4) cp = b0 & 0x07;
    else return 0xFFFD;
    for (std::size_t i = 1; i < n; ++i) cp = (cp << 6) | (static_cast<unsigned char>(s[pos + i]) & 0x3F);
    return cp;
}

[[nodiscard]] inline std::size_t length(std::string_view s) noexcept {
    std::size_t n = 0;
    for (char c : s) n += isContinuation(c) ? 0 : 1;
    return n;
}

/// Byte length of the first `count` code points.
[[nodiscard]] inline std::size_t prefixBytes(std::string_view s, std::size_t count) noexcept {
    std::size_t pos = 0;
    while (count-- > 0 && pos < s.size()) pos = next(s, pos);
    return pos;
}

/// Characters that separate words for Ctrl+Left/Right and line wrapping.
[[nodiscard]] inline bool isSpace(char32_t cp) noexcept {
    return cp == ' ' || cp == '\t' || cp == 0x3000 /* ideographic space */;
}

/// CJK and similar scripts may break between any two characters.
[[nodiscard]] inline bool isWide(char32_t cp) noexcept {
    return (cp >= 0x2E80 && cp <= 0x9FFF) || (cp >= 0xAC00 && cp <= 0xD7AF) || (cp >= 0xF900 && cp <= 0xFAFF)
        || (cp >= 0xFF00 && cp <= 0xFFEF) || (cp >= 0x20000 && cp <= 0x3FFFF);
}

/// Drop control characters and map line breaks / tabs to spaces (single-line input).
[[nodiscard]] inline std::string sanitizeLine(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        std::size_t j  = next(s, i);
        char32_t    cp = decode(s, i);
        if (cp == '\n' || cp == '\t') {
            if (!out.empty() && out.back() != ' ') out += ' ';
        } else if (cp >= 0x20 && cp != 0x7F && !(cp >= 0x80 && cp < 0xA0)) {
            out.append(s.substr(i, j - i));
        }
        i = j;
    }
    return out;
}

/// Start of the word before `pos` (skips spaces first), for Ctrl+Left / Ctrl+Backspace.
[[nodiscard]] inline std::size_t wordLeft(std::string_view s, std::size_t pos) noexcept {
    pos = floorBoundary(s, pos);
    while (pos > 0 && isSpace(decode(s, prev(s, pos)))) pos = prev(s, pos);
    while (pos > 0 && !isSpace(decode(s, prev(s, pos)))) pos = prev(s, pos);
    return pos;
}

/// Start of the next word after `pos`, for Ctrl+Right / Ctrl+Delete.
[[nodiscard]] inline std::size_t wordRight(std::string_view s, std::size_t pos) noexcept {
    pos = floorBoundary(s, pos);
    while (pos < s.size() && !isSpace(decode(s, pos))) pos = next(s, pos);
    while (pos < s.size() && isSpace(decode(s, pos))) pos = next(s, pos);
    return pos;
}

} // namespace bei::ui::utf8
