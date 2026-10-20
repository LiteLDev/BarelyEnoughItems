#include "core/search/SearchEngine.h"

#include <gtest/gtest.h>

#include <string>

using namespace bei::core;

namespace {
std::vector<ItemEntry> sampleItems() {
    return {
        {ItemKey{"minecraft:oak_planks", 0}, "Oak Planks", "Oak Planks", {"minecraft:planks"}, "", 0},
        {ItemKey{"minecraft:stick", 0},      "Stick",      "Stick",      {},                   "", 1},
        {ItemKey{"minecraft:stone", 0},      "Stone",      "Stone",      {},                   "", 2},
    };
}

// A mixed list: English, localized (Chinese), accented and modded entries.
std::vector<ItemEntry> mixedItems() {
    return {
        /*0*/ {
               ItemKey{"minecraft:oak_planks", 0},
               "橡木木板",                                          "Oak Planks",
               {"minecraft:planks", "minecraft:logs"},
               "",                                                                                                  0
        },
        /*1*/
        {ItemKey{"minecraft:stick", 0},       "木棍",         "Stick",        {},                         "", 1},
        /*2*/
        {ItemKey{"minecraft:stone", 0},       "石头",         "Stone",        {"minecraft:stone_bricks"}, "", 2},
        /*3*/
        {ItemKey{"minecraft:green_dye", 0},   "绿色染料",     "Green Dye",    {},                         "", 3},
        /*4*/
        {ItemKey{"minecraft:iron_sword", 0},  "铁剑",         "Iron Sword",   {"minecraft:swords"},       "", 4},
        /*5*/
        {ItemKey{"mymod:creme_brulee", 0},    "Crème Brûlée", "Creme Brulee", {"mymod:food"},             "", 5},
        /*6*/
        {ItemKey{"mymod:oak_wand", 0},        "§aOak Wand",   "Oak Wand",     {},                         "", 6},
        /*7*/
        {ItemKey{"minecraft:wooden_slab", 0}, "木台阶",       "Wooden Slab",  {"minecraft:slabs"},        "", 7},
    };
}

std::vector<std::size_t> run(std::vector<ItemEntry> const& items, std::string_view q) {
    SearchEngine engine;
    engine.setItems(&items);
    return engine.filter(q);
}

using Idx = std::vector<std::size_t>;
} // namespace

// --- Basics ---------------------------------------------------------------------------------------

TEST(SearchEngine, EmptyQueryReturnsAll) {
    auto         items = sampleItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("").size(), 3u);
    EXPECT_EQ(engine.filter("   ").size(), 3u);
    EXPECT_EQ(engine.filter(" \t ").size(), 3u);
}

TEST(SearchEngine, CaseInsensitiveSubstring) {
    auto         items = sampleItems();
    SearchEngine engine;
    engine.setItems(&items);
    auto r = engine.filter("st");
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0], 1u);
    EXPECT_EQ(r[1], 2u);
    EXPECT_EQ(engine.filter("OAK").size(), 1u);
}

TEST(SearchEngine, AllTermsMustMatch) {
    auto         items = sampleItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("oak planks").size(), 1u);
    EXPECT_EQ(engine.filter("oak stone").size(), 0u);
}

TEST(SearchEngine, NoItemsIsSafe) {
    SearchEngine engine;
    EXPECT_TRUE(engine.filter("x").empty());
    EXPECT_TRUE(engine.filter("").empty());
    engine.setItems(nullptr);
    EXPECT_TRUE(engine.filter("x").empty());
}

TEST(SearchEngine, MatchesDisplayEnglishAndIdPath) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "planks"), (Idx{0}));    // English name
    EXPECT_EQ(run(items, "橡木"), (Idx{0}));      // display name
    EXPECT_EQ(run(items, "green_dye"), (Idx{3})); // id path
    // The namespace is not part of plain-term matching.
    EXPECT_EQ(run(items, "minecraft").size(), 0u);
    EXPECT_EQ(run(items, "mymod").size(), 0u);
}

TEST(SearchEngine, ResultsAreInItemOrder) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "oak"), (Idx{0, 6}));
}

// --- Operators -------------------------------------------------------------------------------------

TEST(SearchEngine, OrOperator) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "stick|stone"), (Idx{1, 2}));
    EXPECT_EQ(run(items, "stick | stone"), (Idx{1, 2}));
    EXPECT_EQ(run(items, "stick |stone"), (Idx{1, 2}));
    EXPECT_EQ(run(items, "stick|stone|sword"), (Idx{1, 2, 4}));
    EXPECT_EQ(run(items, "nothing|stone"), (Idx{2}));
}

TEST(SearchEngine, OrBindsTighterThanSpace) {
    auto items = mixedItems();
    // (oak|stick) AND (wand|planks)
    EXPECT_EQ(run(items, "oak|stick wand|planks"), (Idx{0, 6}));
    // (stick) AND (stone|planks): nothing
    EXPECT_EQ(run(items, "stick stone|planks").size(), 0u);
}

TEST(SearchEngine, DanglingOrIsIgnored) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "stick|"), (Idx{1}));
    EXPECT_EQ(run(items, "|stick"), (Idx{1}));
    EXPECT_EQ(run(items, "|"), (Idx{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST(SearchEngine, Negation) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "oak -wand"), (Idx{0}));
    EXPECT_EQ(run(items, "-oak").size(), 6u);
    EXPECT_EQ(run(items, "-oak -stone -stick"), (Idx{3, 4, 5, 7}));
    // A lone '-' (still typing) is ignored.
    EXPECT_EQ(run(items, "oak -"), (Idx{0, 6}));
}

TEST(SearchEngine, NegationIsPerAlternative) {
    auto items = mixedItems();
    // (not oak) OR wand: everything except oak planks.
    EXPECT_EQ(run(items, "-oak|wand"), (Idx{1, 2, 3, 4, 5, 6, 7}));
}

TEST(SearchEngine, HyphenInsideTermIsLiteral) {
    std::vector<ItemEntry> items{
        {ItemKey{"mymod:x_ray", 0}, "X-Ray Goggles", "X-Ray Goggles", {}, "", 0},
        {ItemKey{"mymod:ray", 0},   "Ray",           "Ray",           {}, "", 1},
    };
    EXPECT_EQ(run(items, "x-ray"), (Idx{0}));
}

TEST(SearchEngine, NamespacePrefix) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "@mymod"), (Idx{5, 6}));
    EXPECT_EQ(run(items, "@MyM"), (Idx{5, 6}));
    EXPECT_EQ(run(items, "@minecraft").size(), 6u);
    // Prefix, not substring.
    EXPECT_EQ(run(items, "@mod").size(), 0u);
    EXPECT_EQ(run(items, "-@minecraft"), (Idx{5, 6}));
    EXPECT_EQ(run(items, "@mymod oak"), (Idx{6}));
    // A bare '@' is ignored while typing.
    EXPECT_EQ(run(items, "@").size(), 8u);
}

TEST(SearchEngine, NamespaceDefaultsToMinecraft) {
    std::vector<ItemEntry> items{
        {ItemKey{"apple", 0}, "Apple", "Apple", {}, "", 0}
    };
    EXPECT_EQ(run(items, "@minecraft"), (Idx{0}));
}

TEST(SearchEngine, TagSubstring) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "#planks"), (Idx{0}));
    EXPECT_EQ(run(items, "#minecraft:planks"), (Idx{0}));
    EXPECT_EQ(run(items, "#stone"), (Idx{2}));
    EXPECT_EQ(run(items, "#food"), (Idx{5}));
    EXPECT_EQ(run(items, "#planks|#slabs"), (Idx{0, 7}));
    EXPECT_EQ(run(items, "-#minecraft").size(), 4u); // stick, dye, wand, (creme: mymod:food)
    // Tags never match plain terms and names never match tag terms.
    EXPECT_EQ(run(items, "#oak").size(), 0u);
    EXPECT_EQ(run(items, "logs").size(), 0u);
}

TEST(SearchEngine, IdSubstring) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "$oak_"), (Idx{0, 6}));
    EXPECT_EQ(run(items, "$minecraft:stick"), (Idx{1}));
    EXPECT_EQ(run(items, "$mymod:"), (Idx{5, 6}));
    EXPECT_EQ(run(items, "-$oak_"), (Idx{1, 2, 3, 4, 5, 7}));
    // '$' does not look at names.
    EXPECT_EQ(run(items, "$planks"), (Idx{0}));
    EXPECT_EQ(run(items, "$wooden_slab"), (Idx{7}));
    EXPECT_EQ(run(items, "$木").size(), 0u);
}

TEST(SearchEngine, QuotedLiteral) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "\"oak planks\""), (Idx{0}));
    EXPECT_EQ(run(items, "\"oak wand\""), (Idx{6}));
    EXPECT_EQ(run(items, "\"planks oak\"").size(), 0u);
    // Unterminated quote runs to the end of the query.
    EXPECT_EQ(run(items, "\"oak pl"), (Idx{0}));
    // Quotes can contain '|' and combine with prefixes and OR.
    EXPECT_EQ(run(items, "-\"oak planks\" oak"), (Idx{6}));
    EXPECT_EQ(run(items, "$\"oak_planks\""), (Idx{0}));
    EXPECT_EQ(run(items, "\"iron sword\"|\"oak wand\""), (Idx{4, 6}));
    // Empty quotes are ignored.
    EXPECT_EQ(run(items, "\"\"").size(), 8u);
}

TEST(SearchEngine, QuotedLiteralIsNotPinyin) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "mugun"), (Idx{1}));
    EXPECT_EQ(run(items, "\"mugun\"").size(), 0u);
    EXPECT_EQ(run(items, "\"木棍\""), (Idx{1}));
}

TEST(SearchEngine, QuoteWithPipeInside) {
    std::vector<ItemEntry> items{
        {ItemKey{"mymod:a", 0}, "A|B Switch", "A|B Switch", {}, "", 0},
        {ItemKey{"mymod:b", 0}, "B",          "B",          {}, "", 1},
    };
    EXPECT_EQ(run(items, "\"a|b\""), (Idx{0}));
}

// --- Unicode ---------------------------------------------------------------------------------------

TEST(SearchEngine, DiacriticInsensitive) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "creme"), (Idx{5}));
    EXPECT_EQ(run(items, "crème"), (Idx{5}));
    EXPECT_EQ(run(items, "CRÈME BRÛLÉE"), (Idx{5}));
    EXPECT_EQ(run(items, "brulee"), (Idx{5}));
}

TEST(SearchEngine, UnicodeCaseFolding) {
    std::vector<ItemEntry> items{
        {ItemKey{"mymod:a", 0}, "Дубовые доски", "Oak Planks", {}, "", 0},
        {ItemKey{"mymod:b", 0}, "ΞΥΛΟ",          "Wood",       {}, "", 1},
        {ItemKey{"mymod:c", 0}, "Straße",        "Street",     {}, "", 2},
        {ItemKey{"mymod:d", 0}, "Ёлка",          "Tree",       {}, "", 3},
    };
    EXPECT_EQ(run(items, "ДУБОВЫЕ"), (Idx{0}));
    EXPECT_EQ(run(items, "доски"), (Idx{0}));
    EXPECT_EQ(run(items, "ξυλο"), (Idx{1}));
    EXPECT_EQ(run(items, "strasse"), (Idx{2}));
    EXPECT_EQ(run(items, "STRASSE"), (Idx{2}));
    EXPECT_EQ(run(items, "елка"), (Idx{3}));
}

TEST(SearchEngine, FullwidthAndIdeographicSpace) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "ＯＡＫ"), (Idx{0, 6}));
    EXPECT_EQ(run(items, "oak　wand"), (Idx{6}));
    EXPECT_EQ(run(items, "stick｜stone"), (Idx{1, 2}));
}

TEST(SearchEngine, FormattingCodesIgnored) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "oak wand"), (Idx{6}));
    EXPECT_EQ(run(items, "aoak").size(), 0u);
}

TEST(SearchEngine, MultibyteNotCorruptedByLowercasing) {
    // 'Ā' (U+0100) is C4 80; 'Ġ' (U+0120) is C4 A0. Byte-wise lowercasing would corrupt these.
    std::vector<ItemEntry> items{
        {ItemKey{"mymod:a", 0}, "ĀĠ",   "x", {}, "", 0},
        {ItemKey{"mymod:b", 0}, "\xC4", "y", {}, "", 1}, // invalid UTF-8 must not crash
    };
    EXPECT_EQ(run(items, "ag"), (Idx{0}));
    EXPECT_EQ(run(items, "āġ"), (Idx{0}));
    (void)run(items, "\xC4"); // invalid UTF-8 in the query must not crash either
}

TEST(SearchEngine, CjkSubstringOnCharacterBoundaries) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "木"), (Idx{0, 1, 7}));
    EXPECT_EQ(run(items, "木板"), (Idx{0}));
    EXPECT_EQ(run(items, "染料"), (Idx{3}));
}

// --- Pinyin ----------------------------------------------------------------------------------------

TEST(SearchEngine, PinyinFull) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "muban"), (Idx{0}));
    EXPECT_EQ(run(items, "xiangmumuban"), (Idx{0}));
    EXPECT_EQ(run(items, "shitou"), (Idx{2}));
    EXPECT_EQ(run(items, "tiejian"), (Idx{4}));
    EXPECT_EQ(run(items, "MUBAN"), (Idx{0}));
}

TEST(SearchEngine, PinyinInitials) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "mb"), (Idx{0}));
    EXPECT_EQ(run(items, "xmmb"), (Idx{0}));
    EXPECT_EQ(run(items, "st"), (Idx{1, 2})); // Stick (English) and 石头 (shi tou)
    EXPECT_EQ(run(items, "sht"), (Idx{2}));
}

TEST(SearchEngine, PinyinPrefixAcrossCharacters) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "m"), (Idx{0, 1, 5, 7})); // 木 (mu) and "creme"
    EXPECT_EQ(run(items, "mu"), (Idx{0, 1, 7}));
    EXPECT_EQ(run(items, "mub"), (Idx{0}));
    EXPECT_EQ(run(items, "muba"), (Idx{0}));
    EXPECT_EQ(run(items, "mubanx").size(), 0u);
    EXPECT_EQ(run(items, "mug"), (Idx{1}));
    EXPECT_EQ(run(items, "mutai"), (Idx{7}));
    EXPECT_EQ(run(items, "mtj"), (Idx{7}));
}

TEST(SearchEngine, PinyinMixedInitialsAndFull) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "mban"), (Idx{0}));
    EXPECT_EQ(run(items, "xiangmmb"), (Idx{0}));
    EXPECT_EQ(run(items, "zhang").size(), 0u);
}

TEST(SearchEngine, PinyinMixedWithHanzi) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "木ban"), (Idx{0}));
    EXPECT_EQ(run(items, "mu板"), (Idx{0}));
    EXPECT_EQ(run(items, "橡mmb"), (Idx{0}));
}

TEST(SearchEngine, PinyinUmlautAsV) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "lvse"), (Idx{3}));
    EXPECT_EQ(run(items, "lsrl"), (Idx{3}));
    // 绿 also reads "lu" (polyphone).
    EXPECT_EQ(run(items, "luse"), (Idx{3}));
}

TEST(SearchEngine, PinyinPolyphones) {
    std::vector<ItemEntry> items{
        {ItemKey{"mymod:a", 0}, "长剑", "Long Sword", {}, "", 0},
        {ItemKey{"mymod:b", 0}, "银行", "Bank",       {}, "", 1},
    };
    EXPECT_EQ(run(items, "changjian"), (Idx{0}));
    EXPECT_EQ(run(items, "zhangjian"), (Idx{0}));
    EXPECT_EQ(run(items, "yinhang"), (Idx{1}));
    EXPECT_EQ(run(items, "yinxing"), (Idx{1}));
}

TEST(SearchEngine, PinyinWithLatinInName) {
    std::vector<ItemEntry> items{
        {ItemKey{"minecraft:tnt", 0}, "TNT矿车", "Minecart with TNT", {}, "", 0},
    };
    EXPECT_EQ(run(items, "tntkc"), (Idx{0}));
    EXPECT_EQ(run(items, "tntkuangche"), (Idx{0}));
    EXPECT_EQ(run(items, "ntk"), (Idx{0}));
}

TEST(SearchEngine, PinyinDoesNotStartMidSyllable) {
    auto items = mixedItems();
    // "uban" is a substring of "muban" but does not start at a character's reading.
    EXPECT_EQ(run(items, "uban").size(), 0u);
}

TEST(SearchEngine, PinyinWithOperators) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "mu -mb"), (Idx{1, 7}));
    EXPECT_EQ(run(items, "mg|st"), (Idx{1, 2}));
    EXPECT_EQ(run(items, "@minecraft mb"), (Idx{0}));
}

TEST(SearchEngine, MatchesSingleEntry) {
    auto         items = mixedItems();
    SearchEngine engine;
    EXPECT_TRUE(engine.matches(items[0], "mb"));
    EXPECT_TRUE(engine.matches(items[0], "#planks oak"));
    EXPECT_FALSE(engine.matches(items[0], "-oak"));
    EXPECT_TRUE(engine.matches(items[0], ""));
    EXPECT_TRUE(engine.matches(items[5], "@mymod brulee"));
}

// --- Parser ----------------------------------------------------------------------------------------

TEST(SearchEngineParse, Structure) {
    auto q = SearchEngine::parse(" a|-@B  #c \"d e\"|$f ");
    ASSERT_EQ(q.clauses.size(), 3u);
    ASSERT_EQ(q.clauses[0].terms.size(), 2u);
    EXPECT_EQ(q.clauses[0].terms[0].text, "a");
    EXPECT_EQ(q.clauses[0].terms[1].kind, SearchEngine::Term::Kind::Namespace);
    EXPECT_TRUE(q.clauses[0].terms[1].negated);
    EXPECT_EQ(q.clauses[0].terms[1].text, "b");
    ASSERT_EQ(q.clauses[1].terms.size(), 1u);
    EXPECT_EQ(q.clauses[1].terms[0].kind, SearchEngine::Term::Kind::Tag);
    ASSERT_EQ(q.clauses[2].terms.size(), 2u);
    EXPECT_TRUE(q.clauses[2].terms[0].quoted);
    EXPECT_EQ(q.clauses[2].terms[0].text, "d e");
    EXPECT_EQ(q.clauses[2].terms[1].kind, SearchEngine::Term::Kind::Id);
    EXPECT_EQ(q.clauses[2].terms[1].text, "f");
}

TEST(SearchEngineParse, EmptyAndDangling) {
    EXPECT_TRUE(SearchEngine::parse("").clauses.empty());
    EXPECT_TRUE(SearchEngine::parse(" - @ # $ \"\" | ").clauses.empty());
}

// --- Cache -----------------------------------------------------------------------------------------

TEST(SearchEngineCache, KeysBuiltOncePerSetItems) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.keyBuildCount(), items.size());
    (void)engine.filter("oak");
    (void)engine.filter("stone");
    // Re-setting the same unchanged list does not recompute anything.
    engine.setItems(&items);
    EXPECT_EQ(engine.keyBuildCount(), items.size());
}

TEST(SearchEngineCache, ChangedEntriesAreRebuilt) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("stick"), (Idx{1}));
    items[1].displayName = "Staff";
    items[1].englishName = "Staff";
    engine.setItems(&items);
    EXPECT_EQ(engine.keyBuildCount(), items.size() + 1);
    // The cached "stick" result must not be reused for its refinement.
    EXPECT_EQ(engine.filter("stick"), (Idx{1})); // id path still "stick"
    EXPECT_EQ(engine.filter("staff"), (Idx{1}));
    EXPECT_EQ(engine.lastScanCount(), items.size());
    items[1].tags = {"mymod:staves"};
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("#staves"), (Idx{1}));
}

TEST(SearchEngineCache, LanguageChangeRebuildsKeys) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("mb"), (Idx{0}));
    for (auto& e : items) e.displayName = e.englishName;
    engine.setItems(&items);
    EXPECT_TRUE(engine.filter("mb").empty());
    EXPECT_EQ(engine.filter("muban").size(), 0u);
}

TEST(SearchEngineCache, ResizedListIsRebuilt) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("oak").size(), 2u);
    items.erase(items.begin()); // shifts everything
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("oak"), (Idx{5}));
    EXPECT_EQ(engine.filter("stick"), (Idx{0}));
    items.push_back({
        ItemKey{"minecraft:oak_log", 0},
        "橡木原木",
        "Oak Log",
        {},
        "",
        8
    });
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("oak"), (Idx{5, 7}));
}

TEST(SearchEngineCache, ExtendedQueryScansOnlyPreviousResult) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("m"), (Idx{0, 1, 5, 7}));
    EXPECT_EQ(engine.lastScanCount(), items.size());
    EXPECT_EQ(engine.filter("mu"), (Idx{0, 1, 7}));
    EXPECT_EQ(engine.lastScanCount(), 4u);
    EXPECT_EQ(engine.filter("mub"), (Idx{0}));
    EXPECT_EQ(engine.lastScanCount(), 3u);
    EXPECT_EQ(engine.filter("mub "), (Idx{0})); // trailing space: same query
    EXPECT_EQ(engine.lastScanCount(), 1u);
    EXPECT_EQ(engine.filter("mub @mine"), (Idx{0})); // adding an AND clause narrows
    EXPECT_EQ(engine.lastScanCount(), 1u);
    // Backspace widens: full scan again.
    EXPECT_EQ(engine.filter("mu"), (Idx{0, 1, 7}));
    EXPECT_EQ(engine.lastScanCount(), items.size());
}

TEST(SearchEngineCache, OrAndNegationAreNotTreatedAsNarrowing) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("stick"), (Idx{1}));
    // "stick|" parses like "stick" ...
    EXPECT_EQ(engine.filter("stick|"), (Idx{1}));
    // ... but "stick|s" widens and must rescan everything.
    EXPECT_EQ(engine.filter("stick|stone"), (Idx{1, 2}));
    EXPECT_EQ(engine.lastScanCount(), items.size());

    EXPECT_EQ(engine.filter("oak -w"), (Idx{0}));
    // Extending a negated term widens the result.
    EXPECT_EQ(engine.filter("oak -wx"), (Idx{0, 6}));
    EXPECT_EQ(engine.lastScanCount(), items.size());
}

TEST(SearchEngineCache, ExtendingOrAlternativesNarrows) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("sti|sto"), (Idx{1, 2}));
    EXPECT_EQ(engine.filter("stic|sto"), (Idx{1, 2}));
    EXPECT_EQ(engine.lastScanCount(), 2u);
    EXPECT_EQ(engine.filter("stic|ston"), (Idx{1, 2}));
    EXPECT_EQ(engine.lastScanCount(), 2u);
}

TEST(SearchEngineCache, IncrementalMatchesFullScan) {
    // For every prefix of several queries, the incremental result must equal a fresh engine's result.
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items);
    for (std::string q : {"xiangmumuban", "oak -wand|stick", "@mymod \"crème b", "#minecraft:pl|$sti", "mu 木ban"}) {
        for (std::size_t n = 0; n <= q.size(); ++n) {
            auto prefix = q.substr(0, n);
            EXPECT_EQ(engine.filter(prefix), run(items, prefix)) << "query prefix: '" << prefix << "'";
        }
    }
}

TEST(SearchEngineCache, SwitchingListsInvalidatesCache) {
    auto         a = mixedItems();
    auto         b = sampleItems();
    SearchEngine engine;
    engine.setItems(&a);
    EXPECT_EQ(engine.filter("st"), (Idx{1, 2}));
    engine.setItems(&b);
    EXPECT_EQ(engine.filter("sto"), (Idx{2}));
    EXPECT_EQ(engine.lastScanCount(), b.size());
}

// --- Review follow-ups -----------------------------------------------------------------------------

TEST(SearchEngine, PlainTermWithColonMatchesFullId) {
    auto items = mixedItems();
    EXPECT_EQ(run(items, "minecraft:stick"), (Idx{1}));
    EXPECT_EQ(run(items, "mymod:"), (Idx{5, 6}));
    // Without ':' the namespace is still not searched.
    EXPECT_TRUE(run(items, "mymod").empty());
}

TEST(SearchEngineParse, DroppedEmptyTermEndsOrChain) {
    // "oak|- stone" while typing: the dangling "|-" must not turn "stone" into an alternative of "oak".
    auto q = SearchEngine::parse("oak|- stone");
    ASSERT_EQ(q.clauses.size(), 2u);
    EXPECT_EQ(q.clauses[0].terms.size(), 1u);
    EXPECT_EQ(q.clauses[1].terms.size(), 1u);
    auto items = mixedItems();
    EXPECT_TRUE(run(items, "oak|@ stone").empty());
}

TEST(SearchEngineParse, SectionSignInQueryIsLiteral) {
    auto q = SearchEngine::parse("a§bc");
    ASSERT_EQ(q.clauses.size(), 1u);
    EXPECT_EQ(q.clauses[0].terms[0].text, "a§bc");
    std::vector<ItemEntry> items{
        {ItemKey{"mymod:ac", 0}, "ac", "ac", {}, "", 0}
    };
    EXPECT_TRUE(run(items, "a§bc").empty());
}

TEST(SearchEngineCache, RevisionOverloadSkipsChangeCheck) {
    auto         items = mixedItems();
    SearchEngine engine;
    engine.setItems(&items, 1);
    EXPECT_EQ(engine.keyBuildCount(), items.size());
    EXPECT_EQ(engine.filter("stick"), (Idx{1}));
    // Same revision: trusted, nothing is re-checked (the caller promised nothing changed).
    items[1].englishName = "Staff";
    engine.setItems(&items, 1);
    EXPECT_EQ(engine.keyBuildCount(), items.size());
    // New revision: changed entries are rebuilt and the query cache is dropped.
    engine.setItems(&items, 2);
    EXPECT_EQ(engine.keyBuildCount(), items.size() + 1);
    EXPECT_EQ(engine.filter("staff"), (Idx{1}));
    EXPECT_EQ(engine.lastScanCount(), items.size());
    // The plain overload always re-checks.
    items[2].englishName = "Rock";
    engine.setItems(&items);
    EXPECT_EQ(engine.filter("rock"), (Idx{2}));
}
