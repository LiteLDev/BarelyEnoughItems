#include "core/search/TextFold.h"
#include "core/search/Pinyin.h"

#include <gtest/gtest.h>

using namespace bei::core;

TEST(TextFold, AsciiLowercase) {
    EXPECT_EQ(text::fold("Oak PLANKS 123_:"), "oak planks 123_:");
    EXPECT_EQ(text::fold(""), "");
}

TEST(TextFold, LatinDiacritics) {
    EXPECT_EQ(text::fold("ÀÉÎÕÜ àéîõü Çç Ññ Ÿÿ Øø"), "aeiou aeiou cc nn yy oo");
    EXPECT_EQ(text::fold("Æsir Œuvre Straße Ĳssel"), "aesir oeuvre strasse ijssel");
    EXPECT_EQ(text::fold("ĀāĞğİıŁłŐőŠšŽž"), "aaggiilloosszz");
    // Combining marks (decomposed form) are dropped.
    EXPECT_EQ(text::fold("é"), "e");
}

TEST(TextFold, KeepsUnrelatedSymbols) {
    EXPECT_EQ(text::fold("2×3÷1"), "2×3÷1");
    EXPECT_EQ(text::fold("Þ"), "þ");
}

TEST(TextFold, GreekAndCyrillic) {
    EXPECT_EQ(text::fold("ΆΣΟΣ ς"), "ασοσ σ");
    EXPECT_EQ(text::fold("ЁЖИК Ѓ"), "ежик ѓ");
}

TEST(TextFold, FullwidthAndSpaces) {
    EXPECT_EQ(text::fold("ＡＢＣ１２３｜＃"), "abc123|#");
    EXPECT_EQ(text::fold("a　b c"), "a b c");
}

TEST(TextFold, CjkUnchanged) { EXPECT_EQ(text::fold("木板ABC"), "木板abc"); }

TEST(TextFold, FormattingCodesStripped) {
    EXPECT_EQ(text::fold("§aGreen§r Text"), "green text");
    EXPECT_EQ(text::fold("§"), "");
    EXPECT_EQ(text::fold("§木x"), "x");
}

TEST(TextFold, InvalidUtf8BecomesReplacementChar) {
    EXPECT_EQ(text::fold("a\xC4"), "a�");
    EXPECT_EQ(text::fold("\xC0\xAF"), "��");      // overlong
    EXPECT_EQ(text::fold("\xED\xA0\x80"), "���"); // surrogate
    EXPECT_EQ(
        text::fold(
            "\xFF"
            "A"
        ),
        "�a"
    );
}

TEST(TextFold, DecodeAndEncodeRoundTrip) {
    for (char32_t cp : {U'a', U'é', U'木', U'\U0001F600'}) {
        std::string s;
        text::appendUtf8(s, cp);
        std::size_t pos = 0;
        EXPECT_EQ(text::decodeUtf8(s, pos), cp);
        EXPECT_EQ(pos, s.size());
    }
}

TEST(Pinyin, PrimaryReadings) {
    EXPECT_EQ(pinyin::toPinyin("木板"), "muban");
    EXPECT_EQ(pinyin::toPinyin("橡木木板"), "xiangmumuban");
    EXPECT_EQ(pinyin::toPinyin("钻石剑"), "zuanshijian");
    EXPECT_EQ(pinyin::toPinyin("绿色"), "lvse");
    EXPECT_EQ(pinyin::toPinyin("TNT矿车"), "TNTkuangche");
}

TEST(Pinyin, CoversWholeUnifiedBlock) {
    std::size_t missing = 0;
    for (char32_t cp = 0x4E00; cp <= 0x9FA5; ++cp) missing += pinyin::hasReading(cp) ? 0 : 1;
    // The original GB/Unicode 1.1 block is essentially fully covered.
    EXPECT_LT(missing, 50u);
    EXPECT_TRUE(pinyin::hasReading(0x3400)); // Extension A
    EXPECT_FALSE(pinyin::hasReading(U'a'));
    EXPECT_FALSE(pinyin::hasReading(0x3007));
    EXPECT_FALSE(pinyin::hasReading(0x20000));
}

TEST(Pinyin, Polyphones) {
    auto has = [](char32_t cp, std::string_view syl) {
        auto r = pinyin::readingsOf(cp);
        for (std::size_t i = 0; i < r.count; ++i) {
            if (pinyin::syllable(r.ids[i]) == syl) return true;
        }
        return false;
    };
    EXPECT_TRUE(has(U'长', "chang"));
    EXPECT_TRUE(has(U'长', "zhang"));
    EXPECT_TRUE(has(U'行', "xing"));
    EXPECT_TRUE(has(U'行', "hang"));
    EXPECT_TRUE(has(U'重', "zhong"));
    EXPECT_TRUE(has(U'重', "chong"));
    EXPECT_LE(pinyin::readingsOf(U'行').count, pinyin::kMaxReadings);
}

TEST(Pinyin, SyllableBounds) {
    EXPECT_EQ(pinyin::syllable(0), "");
    EXPECT_EQ(pinyin::syllable(0xFFFF), "");
}
