// Scaffold implementation: creative inventory order, localized hover names, no tags,
// no English names, no extra registry items. Unit "Item catalog" completes it.
#include "game/items/ItemCatalog.h"

#include "game/Game.h"
#include "mod/Log.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"

#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/registry/CreativeItemEntry.h"
#include "mc/world/item/registry/CreativeItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"

namespace bei::game {

namespace {
ll::event::ListenerPtr gJoin;
ll::event::ListenerPtr gExit;
} // namespace

ItemCatalog& ItemCatalog::get() {
    static ItemCatalog instance;
    return instance;
}

void ItemCatalog::install() {
    auto& bus = ll::event::EventBus::getInstance();
    gJoin     = bus.emplaceListener<ll::event::ClientJoinLevelEvent>([this](auto&) { rebuild(); });
    gExit     = bus.emplaceListener<ll::event::ClientExitLevelEvent>([this](auto&) { clear(); });
}

void ItemCatalog::uninstall() {
    auto& bus = ll::event::EventBus::getInstance();
    if (gJoin) bus.removeListener(gJoin);
    if (gExit) bus.removeListener(gExit);
    gJoin.reset();
    gExit.reset();
    clear();
}

void ItemCatalog::clear() {
    mEntries.clear();
    mIndex.clear();
    mStacks.clear();
    ++mGeneration;
}

core::ItemKey ItemCatalog::keyOf(ItemStackBase const& stack) {
    if (stack.isNull() || !stack.mItem) return {};
    return core::ItemKey{stack.getTypeName(), stack.getAuxValue()};
}

std::unique_ptr<ItemStack> ItemCatalog::makeStack(core::ItemKey const& key, int count) {
    if (key.empty()) return nullptr;
    auto stack = std::make_unique<ItemStack>();
    stack->reinit(key.id, count, key.isWildcard() ? 0 : key.aux);
    if (stack->isNull()) return nullptr;
    return stack;
}

void ItemCatalog::rebuild() {
    clear();
    auto* level = currentLevel();
    if (!level) return;
    auto registry = level->getItemRegistry().getCreativeItemRegistry();
    if (!registry) return;
    int order = 0;
    for (auto& entry : registry->mCreativeItems.get()) {
        ItemInstance const& inst = entry.mItemInstance;
        if (inst.isNull() || !inst.mItem) continue;
        core::ItemKey key{inst.getTypeName(), inst.getAuxValue()};
        if (key.empty() || mIndex.contains(key)) continue;
        core::ItemEntry e;
        e.key         = key;
        e.displayName = inst.getHoverName();
        e.order       = order++;
        mIndex.emplace(key, mEntries.size());
        mEntries.push_back(std::move(e));
    }
    log().info("Item catalog: {} items", mEntries.size());
}

core::ItemEntry const* ItemCatalog::find(core::ItemKey const& key) const {
    auto it = mIndex.find(key);
    return it == mIndex.end() ? nullptr : &mEntries[it->second];
}

ItemStack const* ItemCatalog::displayStack(core::ItemKey const& key) {
    core::ItemKey k = key;
    if (k.isWildcard()) k.aux = 0;
    if (auto it = mStacks.find(k); it != mStacks.end()) return it->second.get();
    auto  stack = makeStack(k);
    auto* ptr   = stack.get();
    mStacks.emplace(k, std::move(stack)); // cache misses too (nullptr)
    return ptr;
}

std::string ItemCatalog::displayName(core::ItemKey const& key) {
    if (auto* e = find(key)) return e->displayName;
    if (auto* s = displayStack(key)) return s->getHoverName();
    return key.toString();
}

} // namespace bei::game
