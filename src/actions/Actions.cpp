#include "actions/Actions.h"

#include "actions/TransferPlanner.h"
#include "app/I18n.h"
#include "game/Game.h"
#include "game/items/ItemCatalog.h"
#include "mod/Log.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/gui/screens/controllers/CraftingScreenController.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/common/Globals.h"
#include "mc/network/packet/CommandRequestPacket.h"
#include "mc/server/commands/CommandOriginData.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/containers/ContainerEnumName.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <string>

#ifdef BEI_DEV
namespace bei::actions::dev {
void install();
void uninstall();
} // namespace bei::actions::dev
#endif

namespace bei::actions {

namespace {

// --- Tracking the live crafting screen -------------------------------------------------------

/// The most recent live CraftingScreenController (inventory or crafting table). Set by its tick,
/// cleared when it terminates or is destroyed, so it never dangles.
CraftingScreenController* gCrafting = nullptr;

#ifdef BEI_DEV
void dumpCollections();
#endif

void setCrafting(CraftingScreenController* ctrl) {
    if (gCrafting == ctrl) return;
    gCrafting = ctrl;
    if (ctrl) log().debug("Actions: crafting screen active");
    else log().debug("Actions: crafting screen gone");
#ifdef BEI_DEV
    if (ctrl) dumpCollections();
    else log().info("Actions[dev]: crafting screen gone");
#endif
}

LL_TYPE_INSTANCE_HOOK(
    CraftingTickHook,
    ll::memory::HookPriority::Normal,
    CraftingScreenController,
    &CraftingScreenController::$tick,
    ui::DirtyFlag
) {
    setCrafting(this);
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    CraftingTerminateHook,
    ll::memory::HookPriority::Normal,
    CraftingScreenController,
    &CraftingScreenController::$onTerminate,
    void
) {
    if (gCrafting == this) setCrafting(nullptr);
    origin();
}

LL_TYPE_INSTANCE_HOOK(
    ContainerScreenDtorHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$dtor,
    void
) {
    if (gCrafting && static_cast<ContainerScreenController*>(gCrafting) == this) setCrafting(nullptr);
    origin();
}

// --- Container access ------------------------------------------------------------------------

std::string const& collectionName(ContainerEnumName name) {
    // Linear scan: map.find() would hash with our std::hash, which does not match the game's
    // bucket layout (find() misses even though the entry exists).
    static std::string const empty;
    for (auto const& [key, value] : ContainerCollectionNameMap()) {
        if (key == name) return value;
    }
    return empty;
}

ContainerManagerController* manager() {
    if (!gCrafting || !game::currentPlayer()) return nullptr;
    return gCrafting->mContainerManagerController.get();
}

struct Slot {
    std::string   collection;
    int           index = 0;
    core::ItemKey key;
    int           count    = 0;
    int           maxStack = 64;
};

/// Collections that hold the player's own items in the crafting screen.
std::vector<std::string> inventoryCollections(ContainerManagerController& mgr) {
    std::vector<std::string> names;
    auto const&              combined = collectionName(ContainerEnumName::CombinedHotbarAndInventoryContainer);
    if (!combined.empty() && mgr.hasContainerController(combined)) {
        names.push_back(combined);
        return names;
    }
    for (auto e : {ContainerEnumName::HotbarContainer, ContainerEnumName::InventoryContainer}) {
        auto const& n = collectionName(e);
        if (!n.empty() && mgr.hasContainerController(n)) names.push_back(n);
    }
    return names;
}

std::string const& gridCollection() { return collectionName(ContainerEnumName::CraftingInputContainer); }

void collectSlots(ContainerManagerController& mgr, std::string const& collection, std::vector<Slot>& out) {
    if (collection.empty() || !mgr.hasContainerController(collection)) return;
    int const size = mgr.getContainerSize(collection);
    for (int i = 0; i < size; ++i) {
        auto const& stack = mgr.getItemStack(collection, i);
        auto        key   = game::ItemCatalog::keyOf(stack);
        if (key.empty() || stack.mCount == 0) continue;
        out.push_back({collection, i, std::move(key), static_cast<int>(stack.mCount), stack.getMaxStackSize()});
    }
}

std::vector<Slot> inventorySlots(ContainerManagerController& mgr) {
    std::vector<Slot> slots;
    for (auto const& name : inventoryCollections(mgr)) collectSlots(mgr, name, slots);
    return slots;
}

std::vector<Slot> gridSlots(ContainerManagerController& mgr) {
    std::vector<Slot> slots;
    collectSlots(mgr, gridCollection(), slots);
    return slots;
}

std::vector<PoolEntry> toPool(std::vector<Slot> const& slots) {
    std::vector<PoolEntry> pool;
    for (auto const& s : slots) addToPool(pool, s.key, s.count, s.maxStack);
    return pool;
}

int gridSizeOf(ContainerManagerController& mgr) {
    auto const& grid = gridCollection();
    if (grid.empty() || !mgr.hasContainerController(grid)) return 0;
    int const cells = mgr.getContainerSize(grid);
    int const side  = static_cast<int>(std::lround(std::sqrt(static_cast<double>(std::max(cells, 0)))));
    return side >= 2 && side * side == cells ? side : 0;
}

#ifdef BEI_DEV
void dumpCollections() {
    auto* mgr = manager();
    log().info("Actions[dev]: crafting screen active, manager={}", static_cast<void*>(mgr));
    if (!mgr) return;
    for (auto const& [e, name] : ContainerCollectionNameMap()) {
        if (!mgr->hasContainerController(name)) continue;
        log().info("Actions[dev]:   {} ({}) size={}", name, static_cast<int>(e), mgr->getContainerSize(name));
    }
    auto const& cursor = ContainerScreenController::mCursorSlotData();
    log().info("Actions[dev]:   cursor slot {}#{}", cursor.mCollectionName.get(), cursor.mCollectionIndex);
}
#endif

ItemStack const* cursorStack(ContainerManagerController& mgr) {
    auto const& cursor = ContainerScreenController::mCursorSlotData();
    if (!mgr.hasContainerController(cursor.mCollectionName.get())) return nullptr;
    auto const& stack = mgr.getItemStack(cursor.mCollectionName.get(), cursor.mCollectionIndex);
    return stack.isNull() || stack.mCount == 0 ? nullptr : &stack;
}

// --- Commands --------------------------------------------------------------------------------

/// Same layout as CommandRequestPacketPayload, whose default constructor is not exported.
struct CommandRequestPayloadData {
    ::ll::TypedStorage<8, 32, ::std::string>       mCommand;
    ::ll::TypedStorage<8, 64, ::CommandOriginData> mOrigin;
    ::ll::TypedStorage<4, 4, ::CurrentCmdVersion>  mVersion;
    ::ll::TypedStorage<1, 1, bool>                 mInternalSource;
};
static_assert(sizeof(CommandRequestPayloadData) == sizeof(CommandRequestPacketPayload));
static_assert(alignof(CommandRequestPayloadData) == alignof(CommandRequestPacketPayload));
static_assert(
    offsetof(CommandRequestPayloadData, mInternalSource) == offsetof(CommandRequestPacketPayload, mInternalSource)
);

/// Send a command to the server exactly like the chat screen does (a CommandRequestPacket from
/// the player origin). The server checks permissions and answers through the normal chat feedback.
bool sendCommand(std::string command) {
    auto* player = game::currentPlayer();
    if (!player) return false;
    CommandRequestPayloadData data;
    data.mCommand           = std::move(command);
    data.mOrigin->mType     = CommandOriginType::Player;
    data.mOrigin->mUUID     = player->getUuid();
    data.mOrigin->mPlayerId = 0; // only serialized for dev-console/test origins
    data.mVersion           = CurrentCmdVersion::Latest;
    data.mInternalSource    = false;
    // Allocated and released through the virtual destructor: the packet's own vftable is not exported.
    std::unique_ptr<Packet> packet{
        new CommandRequestPacket(reinterpret_cast<CommandRequestPacketPayload const&>(data))
    };
    packet->sendToServer();
    return true;
}

bool isValidItemId(std::string_view id) {
    if (id.empty() || id.size() > 256) return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ':'
            || c == '.' || c == '-' || c == '/';
    });
}

} // namespace

Actions& Actions::get() {
    static Actions instance;
    return instance;
}

void Actions::install() {
    CraftingTickHook::hook();
    CraftingTerminateHook::hook();
    ContainerScreenDtorHook::hook();
#ifdef BEI_DEV
    dev::install();
#endif
}

void Actions::uninstall() {
#ifdef BEI_DEV
    dev::uninstall();
#endif
    ContainerScreenDtorHook::unhook();
    CraftingTerminateHook::unhook();
    CraftingTickHook::unhook();
    gCrafting = nullptr;
}

int Actions::currentCraftingGridSize() const {
    auto* mgr = manager();
    return mgr ? gridSizeOf(*mgr) : 0;
}

TransferCheck Actions::checkTransfer(core::RecipeView const& recipe) {
    TransferCheck result;
    if (recipe.category != core::category::CRAFTING) return result;
    result.block = TransferBlock::NoGrid;
    auto* mgr    = manager();
    if (!mgr) return result;
    int const gridSize = gridSizeOf(*mgr);
    if (gridSize == 0) return result;
    if (!mapToGrid(recipe, gridSize).fits) {
        // Recipes with no inputs at all are not transferable either; report them as unsupported.
        bool const anyInput =
            std::any_of(recipe.inputs.begin(), recipe.inputs.end(), [](auto& in) { return !in.empty(); });
        result.block = anyInput ? TransferBlock::TooLarge : TransferBlock::Unsupported;
        return result;
    }
    result.possible = true;
    result.block    = TransferBlock::None;

    // Items already in the grid count as available: transfer moves them back first.
    auto slots = inventorySlots(*mgr);
    auto grid  = gridSlots(*mgr);
    slots.insert(slots.end(), grid.begin(), grid.end());
    result.missingInputs = assign(recipe, toPool(slots), 1).missing;
    return result;
}

std::string Actions::transferTooltip(TransferCheck const& check) {
    if (check.canCraft()) {
        return app::tr("bei.actions.transfer.move", "Move items") + "\n"
             + app::tr("bei.actions.transfer.moveAll", "Shift-click: move as many as possible");
    }
    if (check.possible) return app::tr("bei.actions.transfer.missing", "Missing items");
    switch (check.block) {
    case TransferBlock::TooLarge:
        return app::tr("bei.actions.transfer.tooLarge", "This recipe needs a crafting table");
    case TransferBlock::NoGrid:
        return app::tr(
            "bei.actions.transfer.noGrid",
            "Open a crafting table (or the inventory for 2x2 recipes) to move items"
        );
    default:
        return {};
    }
}

bool Actions::transfer(core::RecipeView const& recipe, bool maxSetsWanted) {
    auto check = checkTransfer(recipe);
    if (!check.canCraft()) return false;
    auto* mgr = manager();
    if (!mgr) return false;
    if (cursorStack(*mgr)) {
        log().debug("Actions: transfer refused, an item is on the cursor");
        return false;
    }
    int const gridSize = gridSizeOf(*mgr);
    auto      mapping  = mapToGrid(recipe, gridSize);

    // Clear the grid back into the inventory (vanilla "evacuate", predicted client-side).
    if (!gridSlots(*mgr).empty()) gCrafting->_evacuateCraftingGrid();
    if (!gridSlots(*mgr).empty()) {
        log().warn("Actions: crafting grid could not be cleared (inventory full?)");
        return false;
    }

    auto       inventory = inventorySlots(*mgr);
    auto const pool      = toPool(inventory);
    int const  sets      = maxSetsWanted ? maxSets(recipe, pool) : 1;
    if (sets <= 0) return false;
    auto const plan = assign(recipe, pool, sets);
    if (!plan.complete()) return false;

    auto const& grid = gridCollection();
    bool        ok   = true;
    for (std::size_t i = 0; i < recipe.inputs.size(); ++i) {
        int const cell = mapping.cellOfInput[i];
        if (cell < 0 || plan.chosen[i].empty()) continue;
        int need = std::max(1, recipe.inputs[i].count) * sets;
        for (auto& src : inventory) {
            if (need <= 0) break;
            if (src.count <= 0 || src.key != plan.chosen[i]) continue;
            int const amount = std::min(need, src.count);
            if (!mgr->handlePlaceAmount(SlotData(src.collection, src.index), amount, SlotData(grid, cell))) {
                log().warn("Actions: moving {} x{} into the grid failed", src.key.toString(), amount);
                ok = false;
                break;
            }
            src.count -= amount;
            need      -= amount;
        }
        if (need > 0) ok = false;
        if (!ok) break;
    }
    // Don't leave a half-filled grid behind.
    if (!ok) gCrafting->_evacuateCraftingGrid();
    log().info(
        "Actions: transferred recipe {} ({} set{}) into {}x{} grid: {}",
        recipe.id,
        sets,
        sets == 1 ? "" : "s",
        gridSize,
        gridSize,
        ok ? "ok" : "failed"
    );
    return ok;
}

bool Actions::canCheat() const {
    auto* player = game::currentPlayer();
    auto* level  = game::currentLevel();
    if (!player || !level) return false;
    if (!level->hasCommandsEnabled()) return false;
    // /give needs GameDirectors (operator). Creative alone is not enough on servers.
    return player->getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors;
}

bool Actions::give(core::ItemKey const& item, int count) {
    if (!canCheat() || !isValidItemId(item.id) || count <= 0) return false;
    int const amount = std::clamp(count, 1, 32767);
    int const aux    = item.isWildcard() || item.aux < 0 ? 0 : item.aux;
    auto      cmd    = std::format("/give @s {} {} {}", item.id, amount, aux);
    log().debug("Actions: {}", cmd);
    return sendCommand(std::move(cmd));
}

bool Actions::hasCursorItem() const {
    auto* mgr = manager();
    return mgr && cursorStack(*mgr);
}

bool Actions::canDeleteCursorItem() const {
    auto* player = game::currentPlayer();
    return player && player->isCreative() && canCheat() && hasCursorItem();
}

bool Actions::deleteCursorItem() {
    if (!canDeleteCursorItem()) return false;
    auto* mgr = manager();
    // In creative, placing the cursor stack onto the creative item list destroys it (vanilla behavior,
    // sent as a server-validated destroy request).
    for (auto e :
         {ContainerEnumName::RecipeConstructionContainer,
          ContainerEnumName::RecipeEquipmentContainer,
          ContainerEnumName::RecipeItemsContainer,
          ContainerEnumName::RecipeNatureContainer,
          ContainerEnumName::RecipeSearchContainer}) {
        auto const& name = collectionName(e);
        if (name.empty() || !mgr->hasContainerController(name)) continue;
        gCrafting->_handlePlaceAll(name, 0);
        if (!cursorStack(*mgr)) {
            log().debug("Actions: deleted cursor item via {}", name);
            return true;
        }
    }
    log().debug("Actions: deleting the cursor item failed");
    return false;
}

} // namespace bei::actions
