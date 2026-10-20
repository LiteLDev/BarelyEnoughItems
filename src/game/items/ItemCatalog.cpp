// Item catalog: the creative inventory in order, then registry items missing from it, with
// localized/English names, creative group names and item tags.
#include "game/items/ItemCatalog.h"

#include "game/Game.h"
#include "mod/Log.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/thread/ClientThreadExecutor.h"

#include "mc/client/game/MinecraftGame.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/network/LegacyClientNetworkHandler.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemCommandVisibility.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemTag.h"
#include "mc/world/item/registry/CreativeGroupInfo.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/block/BlockType.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <unordered_set>

namespace bei::game {

namespace {

ll::event::ListenerPtr gJoin;
ll::event::ListenerPtr gExit;
std::atomic<bool>      gInstalled{false};

using namespace std::string_view_literals;

// Ids that are never shown, even if a server puts them into the creative inventory.
constexpr std::array kHiddenIds{
    "minecraft:air"sv,
    "minecraft:unknown"sv,
    "minecraft:info_update"sv,
    "minecraft:info_update2"sv,
    "minecraft:reserved6"sv,
    "minecraft:moving_block"sv,
    "minecraft:movingblock"sv,
    "minecraft:piston_arm_collision"sv,
    "minecraft:sticky_piston_arm_collision"sv,
    "minecraft:client_request_placeholder_block"sv,
};

bool isHiddenId(std::string_view id) { return std::find(kHiddenIds.begin(), kHiddenIds.end(), id) != kHiddenIds.end(); }

// Vanilla registry entries that only exist as a block form (placed/technical states, fluids,
// crops, legacy pre-flattening ids). Only applied to registry items missing from the creative
// inventory; anything a server lists in its creative content is always shown.
constexpr std::array kBlockFormPaths{
    "fire"sv,
    "soul_fire"sv,
    "water"sv,
    "lava"sv,
    "portal"sv,
    "end_portal"sv,
    "end_gateway"sv,
    "bubble_column"sv,
    "frosted_ice"sv,
    "powder_snow"sv,
    "potatoes"sv,
    "carrots"sv,
    "cocoa"sv,
    "trip_wire"sv,
    "redstone_wire"sv,
    "sweet_berry_bush"sv,
    "bamboo_sapling"sv,
    "daylight_detector_inverted"sv,
    "standing_sign"sv,
    "wall_sign"sv,
    "standing_banner"sv,
    "wall_banner"sv,
    "light_block"sv, // legacy; the flattened light_block_<n> ids remain
    "melon_stem"sv,
    "pumpkin_stem"sv,
    "lit_furnace"sv,
    "lit_blast_furnace"sv,
    "lit_smoker"sv,
    "lit_redstone_lamp"sv,
    "lit_redstone_ore"sv,
    "lit_deepslate_redstone_ore"sv,
    "unlit_redstone_torch"sv,
    "powered_comparator"sv,
    "powered_repeater"sv,
    "unpowered_comparator"sv,
    "unpowered_repeater"sv,
};
constexpr std::array kBlockFormPrefixes{
    "item."sv, // item form of a block whose real item id is in the creative inventory
    "flowing_"sv,
    "deprecated_"sv,
    "petrified_oak_"sv,
};
constexpr std::array kBlockFormSuffixes{
    "_wall_sign"sv,
    "_standing_sign"sv,
    "_wall_fan"sv,
    "candle_cake"sv,
    "_crop"sv,
};

bool isBlockFormId(std::string_view id) {
    auto             colon = id.find(':');
    std::string_view path  = colon == std::string_view::npos ? id : id.substr(colon + 1);
    if (std::find(kBlockFormPaths.begin(), kBlockFormPaths.end(), path) != kBlockFormPaths.end()) return true;
    for (auto p : kBlockFormPrefixes)
        if (path.starts_with(p)) return true;
    for (auto s : kBlockFormSuffixes)
        if (path.ends_with(s)) return true;
    // Double slabs come in many spellings (oak_double_slab, double_cut_copper_slab, waxed_double_cut_copper_slab).
    if (path.find("double_") != std::string_view::npos && path.find("slab") != std::string_view::npos) return true;
    return path.find("cave_vines") != std::string_view::npos;
}

/// Hidden from commands, either on the item or (for block items that defer) on its block type.
bool isHiddenInCommands(Item const& item) {
    switch (item.mIsHiddenInCommands) {
    case ItemCommandVisibility::Hidden:
        return true;
    case ItemCommandVisibility::DeferToBlockType: {
        BlockType const* block = item.mBlockType;
        return block && block->mIsHiddenInCommands;
    }
    default:
        return false;
    }
}

std::string lowerAscii(std::string_view s) {
    std::string out{s};
    for (auto& c : out)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}

/// Removes Minecraft formatting codes ("§x") from a name.
std::string stripFormatting(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        // '§' is U+00A7, encoded as C2 A7; the following code point is the format code.
        if (static_cast<unsigned char>(text[i]) == 0xC2 && i + 1 < text.size()
            && static_cast<unsigned char>(text[i + 1]) == 0xA7) {
            i += 2; // now at the format code; skip its whole UTF-8 sequence
            while (i + 1 < text.size() && (static_cast<unsigned char>(text[i + 1]) & 0xC0) == 0x80) ++i;
            continue;
        }
        out.push_back(text[i]);
    }
    return out;
}

/// Translation lookups against one game locale.
class Translator {
public:
    explicit Translator(std::shared_ptr<Localization const> locale) : mLocale(std::move(locale)) {}

    /// Exact lookup; nullopt if the key has no translation.
    [[nodiscard]] std::optional<std::string> lookup(std::string const& key) const {
        if (!mLocale || key.empty()) return std::nullopt;
        std::string out;
        if (!mLocale->get(key, out, kNoParams) || out.empty() || out == key) return std::nullopt;
        return out;
    }

    /// Name for an item description id: tries "<id>.name" (the item/block convention) and "<id>".
    [[nodiscard]] std::optional<std::string> itemName(std::string const& descriptionId) const {
        if (descriptionId.empty()) return std::nullopt;
        if (!descriptionId.ends_with(".name")) {
            if (auto s = lookup(descriptionId + ".name")) return s;
        }
        return lookup(descriptionId);
    }

private:
    static inline std::vector<std::string> const kNoParams{};
    std::shared_ptr<Localization const>          mLocale;
};

std::vector<std::string> tagsOf(Item const& item) {
    std::vector<std::string> tags;
    auto const&              itemTags = item.mTags.get();
    tags.reserve(itemTags.size());
    for (auto const& tag : itemTags) {
        std::string_view s = tag.getString();
        if (s.starts_with('#')) s.remove_prefix(1);
        if (s.empty() || std::find(tags.begin(), tags.end(), s) != tags.end()) continue;
        tags.emplace_back(s);
    }
    return tags;
}

} // namespace

// Creative content arrives (or changes) from the server: rebuild once it has been applied.
LL_TYPE_INSTANCE_HOOK(
    CreativeContentHook,
    ll::memory::HookPriority::Normal,
    LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle,
    void,
    NetworkIdentifier const&     source,
    CreativeContentPacket const& packet
) {
    origin(source, packet);
    ItemCatalog::get().requestRebuild();
}

// The player switched the game language: names must be rebuilt.
LL_TYPE_INSTANCE_HOOK(
    LanguageChangedHook,
    ll::memory::HookPriority::Normal,
    MinecraftGame,
    &MinecraftGame::$onLanguageChanged,
    void,
    std::string const& code,
    bool               languageSystemInitializing
) {
    origin(code, languageSystemInitializing);
    if (!languageSystemInitializing) ItemCatalog::get().requestRebuild();
}

ItemCatalog& ItemCatalog::get() {
    static ItemCatalog instance;
    return instance;
}

ItemCatalog::~ItemCatalog() {
    // Static destruction happens after the game may have torn down its item registry; never
    // run game destructors from here (uninstall()/level exit already cleared the cache).
    for (auto& [key, stack] : mStacks) (void)stack.release();
}

void ItemCatalog::install() {
    auto& bus = ll::event::EventBus::getInstance();
    gJoin     = bus.emplaceListener<ll::event::ClientJoinLevelEvent>([this](auto&) { rebuild(); });
    gExit     = bus.emplaceListener<ll::event::ClientExitLevelEvent>([this](auto&) { clear(); });
    CreativeContentHook::hook();
    LanguageChangedHook::hook();
    gInstalled = true;
}

void ItemCatalog::uninstall() {
    gInstalled = false;
    CreativeContentHook::unhook();
    LanguageChangedHook::unhook();
    auto& bus = ll::event::EventBus::getInstance();
    if (gJoin) bus.removeListener(gJoin);
    if (gExit) bus.removeListener(gExit);
    gJoin.reset();
    gExit.reset();
    clear();
}

void ItemCatalog::requestRebuild() {
    if (!gInstalled.load() || mRebuildQueued.exchange(true)) return;
    ll::thread::ClientThreadExecutor::getDefault().execute([] {
        auto& catalog = ItemCatalog::get();
        if (!catalog.mRebuildQueued.exchange(false) || !gInstalled.load()) return;
        catalog.rebuild();
    });
}

void ItemCatalog::clear() {
    mEntries.clear();
    mIndex.clear();
    mFirstById.clear();
    mStacks.clear();
    mLanguage.clear();
    ++mGeneration;
}

core::ItemKey ItemCatalog::keyOf(ItemStackBase const& stack) {
    if (stack.isNull() || !stack.mItem) return {};
    return core::ItemKey{stack.getTypeName(), stack.getAuxValue()};
}

std::unique_ptr<ItemStack> ItemCatalog::makeStack(core::ItemKey const& key, int count) {
    if (key.empty()) return nullptr;
    count      = std::clamp(count, 1, 255);
    int  aux   = key.isWildcard() || key.aux < 0 ? 0 : key.aux;
    auto stack = std::make_unique<ItemStack>();
    stack->reinit(key.id, count, aux);
    if (aux != 0 && (stack->isNull() || !stack->mItem || !stack->mItem->isValidAuxValue(aux))) {
        // Unknown variant: show the base item rather than nothing.
        stack->reinit(key.id, count, 0);
    }
    if (stack->isNull() || !stack->mItem) return nullptr;
    return stack;
}

void ItemCatalog::rebuild() {
    using Clock = std::chrono::steady_clock;
    auto start  = Clock::now();

    mRebuildQueued = false;
    clear();
    auto* level = currentLevel();
    if (!level) return;
    ItemRegistryRef items    = level->getItemRegistry();
    auto            registry = items.getCreativeItemRegistry();
    if (!registry) return;

    auto& i18n    = getI18n();
    auto  current = i18n.getCurrentLanguage();
    mLanguage     = current->getFullLanguageCode();
    Translator local{current.get()};
    bool       isEnglish = mLanguage == "en_US";
    Translator english{isEnglish ? std::shared_ptr<Localization const>{current.get()} : i18n.getLocaleFor("en_US")};

    // Creative group names, indexed like CreativeItemRegistry::mCreativeGroups.
    auto const&              groups = registry->mCreativeGroups.get();
    std::vector<std::string> groupNames(groups.size());
    for (std::size_t i = 0; i < groups.size(); ++i) {
        auto const& raw = groups[i].mName.get().getString();
        if (raw.empty()) continue;
        if (auto s = local.lookup(raw)) groupNames[i] = stripFormatting(*s);
        else if (auto e = english.lookup(raw)) groupNames[i] = stripFormatting(*e);
    }

    std::unordered_set<Item const*> seenItems;
    int                             order         = 0;
    int                             englishMisses = 0;
    int                             duplicates    = 0;

    auto addEntry = [&](core::ItemKey      key,
                        std::string        displayName,
                        std::string const& descriptionId,
                        Item const&        item,
                        std::string        groupName) {
        core::ItemEntry e;
        e.key         = std::move(key);
        e.displayName = std::move(displayName);
        if (isEnglish) {
            e.englishName = e.displayName;
        } else if (auto en = english.itemName(descriptionId)) {
            e.englishName = stripFormatting(*en);
        } else {
            e.englishName = e.displayName;
            ++englishMisses;
        }
        e.tags      = tagsOf(item);
        e.groupName = std::move(groupName);
        e.order     = order++;
        mFirstById.try_emplace(e.key.id, mEntries.size());
        mIndex.emplace(e.key, mEntries.size());
        mEntries.push_back(std::move(e));
    };

    // 1. Creative inventory, in order.
    int creativeCount = 0;
    for (auto const& entry : registry->mCreativeItems.get()) {
        ItemInstance const& inst = entry.mItemInstance;
        Item const*         item = inst.mItem.get();
        if (inst.isNull() || !item) continue;
        core::ItemKey key{inst.getTypeName(), inst.getAuxValue()};
        if (key.empty() || isHiddenId(key.id)) continue;
        if (mIndex.contains(key)) {
            ++duplicates; // same id/aux with different NBT (enchanted books, fireworks, ...)
            continue;
        }
        std::uint32_t groupIndex = entry.mGroupIndex;
        addEntry(
            key,
            stripFormatting(inst.getHoverName()),
            inst.getDescriptionId(),
            *item,
            groupIndex < groupNames.size() ? groupNames[groupIndex] : std::string{}
        );
        // Keep the creative variant (with its NBT/block state) as the display stack.
        auto stack                          = std::make_unique<ItemStack>();
        static_cast<ItemStackBase&>(*stack) = inst;
        stack->set(1);
        mStacks.emplace(std::move(key), std::move(stack));
        seenItems.insert(item);
        ++creativeCount;
    }

    // 2. Registry items missing from the creative inventory, sorted by id for a stable order.
    std::vector<std::pair<std::string, Item const*>> extras;
    int                                              hidden = 0;
    for (auto const& [name, weak] : items.getNameToItemMap()) {
        Item const* item = weak.get();
        if (!item || seenItems.contains(item)) continue;
        seenItems.insert(item); // aliases map several names to one item
        std::string const& id = item->mFullName.get().getString();
        if (id.empty() || isHiddenId(id) || mFirstById.contains(id)) continue;
        if (isHiddenInCommands(*item) || isBlockFormId(id)) {
            ++hidden;
            continue;
        }
        extras.emplace_back(id, item);
    }
    std::sort(extras.begin(), extras.end(), [](auto const& a, auto const& b) { return a.first < b.first; });

    // Legacy aliases (e.g. "minecraft:log" = "Oak Log") show up under a name that is already listed.
    std::unordered_set<std::string> knownNames;
    knownNames.reserve(mEntries.size() + extras.size());
    for (auto const& e : mEntries) knownNames.insert(lowerAscii(e.displayName));

    int untranslated = 0;
    int sameName     = 0;
    int extraCount   = 0;
    for (auto const& [id, item] : extras) {
        auto stack = std::make_unique<ItemStack>();
        stack->reinit(*item, 1, 0);
        if (stack->isNull() || !stack->mItem) {
            ++untranslated;
            continue;
        }
        // Items without a translation are technical (education-only, placeholders, ...).
        std::string descriptionId = stack->getDescriptionId();
        if (!local.itemName(descriptionId) && !english.itemName(descriptionId)) {
            ++untranslated;
            continue;
        }
        std::string name = stripFormatting(stack->getHoverName());
        // Light blocks share one name but are distinct variants (light level 0..15).
        bool distinctVariant = std::string_view{id}.find(":light_block_") != std::string_view::npos;
        if (!knownNames.insert(lowerAscii(name)).second && !distinctVariant) {
            ++sameName;
            continue;
        }
        core::ItemKey key{id, 0};
        addEntry(key, std::move(name), descriptionId, *item, {});
        mStacks.emplace(std::move(key), std::move(stack));
        ++extraCount;
    }

    auto        ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    std::string registryIds;
    for (std::size_t i = static_cast<std::size_t>(creativeCount); i < mEntries.size(); ++i) {
        if (!registryIds.empty()) registryIds += ", ";
        registryIds += mEntries[i].key.id;
    }
    log().debug("Item catalog registry-only items: {}", registryIds);
    log().info(
        "Item catalog: {} items ({} creative, {} from registry; skipped {} NBT duplicates, {} hidden or block-only, "
        "{} untranslated, {} same-name aliases), "
        "{} groups, language {}, {} English fallbacks, built in {:.1f} ms",
        mEntries.size(),
        creativeCount,
        extraCount,
        duplicates,
        hidden,
        untranslated,
        sameName,
        groups.size(),
        mLanguage,
        englishMisses,
        ms
    );
}

core::ItemEntry const* ItemCatalog::find(core::ItemKey const& key) const {
    auto it = mIndex.find(key);
    return it == mIndex.end() ? nullptr : &mEntries[it->second];
}

core::ItemEntry const* ItemCatalog::findByNameId(std::string_view id) const {
    auto it = mFirstById.find(id);
    return it == mFirstById.end() ? nullptr : &mEntries[it->second];
}

ItemStack const* ItemCatalog::displayStack(core::ItemKey const& key) {
    core::ItemKey k = key;
    if (k.isWildcard()) {
        // Show the first listed variant (e.g. the first creative banner), else aux 0.
        if (auto* e = findByNameId(k.id)) k = e->key;
        else k.aux = 0;
    }
    if (auto it = mStacks.find(k); it != mStacks.end()) return it->second.get();
    auto  stack = makeStack(k);
    auto* ptr   = stack.get();
    mStacks.emplace(k, std::move(stack)); // cache misses too (nullptr)
    return ptr;
}

std::string ItemCatalog::displayName(core::ItemKey const& key) {
    if (auto* e = find(key)) return e->displayName;
    if (auto* s = displayStack(key)) return stripFormatting(s->getHoverName());
    return key.toString();
}

} // namespace bei::game
