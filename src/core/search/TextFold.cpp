#include "core/search/TextFold.h"

namespace bei::core::text {

namespace {

// Base letters for U+00C0..U+00FF. '_' marks code points handled specially in foldCodepoint.
constexpr std::string_view kLatin1 = "aaaaaa_ceeeeiiiidnooooo_ouuuuy__aaaaaa_ceeeeiiiidnooooo_ouuuuy_y";
static_assert(kLatin1.size() == 0x40);

// Base letters for U+0100..U+017F. '_' marks the ligatures IJ/ij and OE/oe.
constexpr std::string_view kLatinExtA = "aaaaaa"
                                        "cccccccc"
                                        "dddd"
                                        "eeeeeeeeee"
                                        "gggggggg"
                                        "hhhh"
                                        "iiiiiiiiii"
                                        "__"
                                        "jj"
                                        "kkk"
                                        "llllllllll"
                                        "nnnnnnnnn"
                                        "oooooo"
                                        "__"
                                        "rrrrrr"
                                        "ssssssss"
                                        "tttttt"
                                        "uuuuuuuuuuuu"
                                        "ww"
                                        "yyy"
                                        "zzzzzz"
                                        "s";
static_assert(kLatinExtA.size() == 0x80);

constexpr bool isCombiningMark(char32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) || (cp >= 0x1DC0 && cp <= 0x1DFF)
        || (cp >= 0x20D0 && cp <= 0x20FF) || (cp >= 0xFE20 && cp <= 0xFE2F);
}

char32_t foldGreek(char32_t cp) {
    switch (cp) {
    case 0x0386:
    case 0x03AC:
        return 0x03B1; // alpha
    case 0x0388:
    case 0x03AD:
        return 0x03B5; // epsilon
    case 0x0389:
    case 0x03AE:
        return 0x03B7; // eta
    case 0x038A:
    case 0x03AA:
    case 0x03AF:
    case 0x0390:
    case 0x03CA:
        return 0x03B9; // iota
    case 0x038C:
    case 0x03CC:
        return 0x03BF; // omicron
    case 0x038E:
    case 0x03AB:
    case 0x03B0:
    case 0x03CB:
    case 0x03CD:
        return 0x03C5; // upsilon
    case 0x038F:
    case 0x03CE:
        return 0x03C9; // omega
    case 0x03C2:
        return 0x03C3; // final sigma
    default:
        break;
    }
    if (cp >= 0x0391 && cp <= 0x03A9 && cp != 0x03A2) return cp + 0x20;
    return cp;
}

char32_t foldCyrillic(char32_t cp) {
    if (cp >= 0x0400 && cp <= 0x040F) cp += 0x50;
    else if (cp >= 0x0410 && cp <= 0x042F) cp += 0x20;
    if (cp == 0x0451) cp = 0x0435; // yo -> ie
    return cp;
}

} // namespace

char32_t decodeUtf8(std::string_view s, std::size_t& pos) {
    auto const b0 = static_cast<unsigned char>(s[pos]);
    if (b0 < 0x80) {
        ++pos;
        return b0;
    }
    std::size_t len;
    char32_t    cp;
    char32_t    minCp;
    if ((b0 & 0xE0) == 0xC0) {
        len = 2, cp = b0 & 0x1F, minCp = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) {
        len = 3, cp = b0 & 0x0F, minCp = 0x800;
    } else if ((b0 & 0xF8) == 0xF0) {
        len = 4, cp = b0 & 0x07, minCp = 0x10000;
    } else {
        ++pos;
        return kReplacementChar;
    }
    if (pos + len > s.size()) {
        ++pos;
        return kReplacementChar;
    }
    for (std::size_t i = 1; i < len; ++i) {
        auto const b = static_cast<unsigned char>(s[pos + i]);
        if ((b & 0xC0) != 0x80) {
            ++pos;
            return kReplacementChar;
        }
        cp = (cp << 6) | (b & 0x3F);
    }
    if (cp < minCp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        ++pos;
        return kReplacementChar;
    }
    pos += len;
    return cp;
}

void appendUtf8(std::string& out, char32_t cp) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = kReplacementChar;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

void foldCodepoint(char32_t cp, std::string& out) {
    if (cp < 0x80) {
        if (cp >= 'A' && cp <= 'Z') cp += 'a' - 'A';
        out.push_back(static_cast<char>(cp));
        return;
    }
    if (isCombiningMark(cp)) return;
    if (cp == 0x00A0 || cp == 0x3000) {
        out.push_back(' ');
        return;
    }
    if (cp >= 0xFF01 && cp <= 0xFF5E) { // fullwidth ASCII
        foldCodepoint(cp - 0xFEE0, out);
        return;
    }
    if (cp >= 0x00C0 && cp <= 0x00FF) {
        char base = kLatin1[cp - 0xC0];
        if (base != '_') {
            out.push_back(base);
        } else if (cp == 0x00C6 || cp == 0x00E6) {
            out += "ae";
        } else if (cp == 0x00DF) {
            out += "ss";
        } else if (cp == 0x00DE) {
            appendUtf8(out, 0x00FE); // thorn: lowercase only
        } else {
            appendUtf8(out, cp); // multiplication/division sign, lowercase thorn
        }
        return;
    }
    if (cp >= 0x0100 && cp <= 0x017F) {
        char base = kLatinExtA[cp - 0x100];
        if (base != '_') out.push_back(base);
        else out += (cp <= 0x0133) ? "ij" : "oe";
        return;
    }
    if (cp >= 0x0386 && cp <= 0x03CE) {
        appendUtf8(out, foldGreek(cp));
        return;
    }
    if (cp >= 0x0400 && cp <= 0x045F) {
        appendUtf8(out, foldCyrillic(cp));
        return;
    }
    appendUtf8(out, cp);
}

std::string fold(std::string_view s, bool stripFormatting) {
    std::string out;
    out.reserve(s.size());
    forEachFolded(s, [&](char32_t, std::string_view piece) { out += piece; }, stripFormatting);
    return out;
}

} // namespace bei::core::text
