#include "core/model/ItemKey.h"

#include <gtest/gtest.h>

using bei::core::ItemKey;

TEST(ItemKey, ToStringAndParseRoundTrip) {
    for (auto const& k : {
             ItemKey{"minecraft:stone", 0},
             ItemKey{"minecraft:wool", 14},
             ItemKey{"minecraft:planks", ItemKey::ANY_AUX},
             ItemKey{"mymod:thing", -1},
         }) {
        auto parsed = ItemKey::parse(k.toString());
        ASSERT_TRUE(parsed.has_value()) << k.toString();
        EXPECT_EQ(*parsed, k);
    }
}

TEST(ItemKey, ParseDefaultsNamespace) {
    auto k = ItemKey::parse("stone");
    ASSERT_TRUE(k);
    EXPECT_EQ(k->id, "minecraft:stone");
    EXPECT_EQ(k->aux, 0);
}

TEST(ItemKey, ParseRejectsMalformed) {
    EXPECT_FALSE(ItemKey::parse(""));
    EXPECT_FALSE(ItemKey::parse(":stone"));
    EXPECT_FALSE(ItemKey::parse("minecraft:"));
    EXPECT_FALSE(ItemKey::parse("minecraft:stone:"));
    EXPECT_FALSE(ItemKey::parse("minecraft:stone:abc"));
    EXPECT_FALSE(ItemKey::parse("minecraft:stone:99999"));
}

TEST(ItemKey, WildcardMatching) {
    ItemKey any{"minecraft:planks", ItemKey::ANY_AUX};
    ItemKey oak{"minecraft:planks", 0};
    ItemKey birch{"minecraft:planks", 2};
    EXPECT_TRUE(any.matches(oak));
    EXPECT_TRUE(any.matches(birch));
    EXPECT_TRUE(oak.matches(any));
    EXPECT_FALSE(oak.matches(birch));
    EXPECT_FALSE(any.matches(ItemKey{"minecraft:stone", 0}));
}

TEST(ItemKey, NamespaceAndPath) {
    ItemKey k{"mymod:copper_gear", 0};
    EXPECT_EQ(k.nameSpace(), "mymod");
    EXPECT_EQ(k.path(), "copper_gear");
    ItemKey bare{"stick", 0};
    EXPECT_EQ(bare.nameSpace(), "minecraft");
    EXPECT_EQ(bare.path(), "stick");
}
