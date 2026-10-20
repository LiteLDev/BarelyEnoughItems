#include "core/search/Pinyin.h"

#include "core/search/TextFold.h"

#include <algorithm>

namespace bei::core::pinyin {

namespace data {
// Defined in the generated PinyinTable.cpp.
extern std::string_view const kSyllables[];
extern std::size_t const      kSyllableCount;
extern std::uint16_t const    kPrimary3400[];
extern std::uint16_t const    kPrimary4E00[];
extern std::uint32_t const    kExtra[];
extern std::size_t const      kExtraCount;
} // namespace data

Readings readingsOf(char32_t cp) {
    Readings      r;
    std::uint16_t primary = 0;
    if (cp >= 0x3400 && cp <= 0x4DBF) primary = data::kPrimary3400[cp - 0x3400];
    else if (cp >= 0x4E00 && cp <= 0x9FFF) primary = data::kPrimary4E00[cp - 0x4E00];
    if (primary == 0) return r;
    r.ids[r.count++] = primary;

    auto const  key = static_cast<std::uint32_t>(cp) << 10;
    auto const* end = data::kExtra + data::kExtraCount;
    for (auto* it = std::lower_bound(data::kExtra, end, key); it != end && (*it >> 10) == cp; ++it) {
        if (r.count == kMaxReadings) break;
        r.ids[r.count++] = static_cast<std::uint16_t>(*it & 0x3FF);
    }
    return r;
}

std::string_view syllable(std::uint16_t id) { return id < data::kSyllableCount ? data::kSyllables[id] : ""; }

std::string toPinyin(std::string_view utf8) {
    std::string out;
    std::size_t pos = 0;
    while (pos < utf8.size()) {
        auto const start = pos;
        char32_t   cp    = text::decodeUtf8(utf8, pos);
        auto       r     = readingsOf(cp);
        if (r.count > 0) out += syllable(r.ids[0]);
        else out.append(utf8.substr(start, pos - start));
    }
    return out;
}

} // namespace bei::core::pinyin
