#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace bei::core::pinyin {

/// Maximum number of readings stored per ideograph (primary + polyphones).
inline constexpr std::size_t kMaxReadings = 3;

/// Syllable ids of an ideograph's readings, primary first. `count == 0` for anything without a reading.
struct Readings {
    std::uint16_t ids[kMaxReadings]{};
    std::size_t   count = 0;
};

/// Readings for a CJK Unified Ideograph (U+3400-U+4DBF, U+4E00-U+9FFF).
/// Data: mozillazg/pinyin-data (MIT), see PinyinTable.cpp.
[[nodiscard]] Readings readingsOf(char32_t cp);

/// Toneless lowercase ASCII syllable for an id from `readingsOf` (u-umlaut is written as `v`).
[[nodiscard]] std::string_view syllable(std::uint16_t id);

/// True if `cp` is an ideograph with at least one known reading.
[[nodiscard]] inline bool hasReading(char32_t cp) { return readingsOf(cp).count > 0; }

/// Primary readings of every ideograph in `utf8`, concatenated; other characters are copied as-is.
/// Mostly useful for debugging and tests ("木板" -> "muban").
[[nodiscard]] std::string toPinyin(std::string_view utf8);

} // namespace bei::core::pinyin
