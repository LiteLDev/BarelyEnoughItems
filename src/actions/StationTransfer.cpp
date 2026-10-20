#include "actions/StationTransfer.h"

#include "actions/TransferPlanner.h"
#include "game/Game.h"
#include "game/items/ItemCatalog.h"
#include "mod/Log.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/FurnaceScreenController.h"
#include "mc/client/gui/screens/controllers/SmithingTableScreenController.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/common/Globals.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/containers/ContainerEnumName.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/item/ItemStack.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace bei::actions::station {

namespace {

// --- Tracking the live station screens ---------------------------------------------------------

/// The live furnace / blast furnace / smoker screen (they share FurnaceScreenController), set by
/// its tick. The live smithing table screen, set by its constructor (it has no tick override).
/// Both are cleared when the controller is destroyed, so they never dangle. They stay valid
/// while another scene (the BEI recipe screen) is pushed on top.
FurnaceScreenController*       gFurnace  = nullptr;
SmithingTableScreenController* gSmithing = nullptr;

std::string const& collectionName(ContainerEnumName name) {
    // Linear scan: the game's map does not hash like our std::hash (see Actions.cpp).
    static std::string const empty;
    for (auto const& [key, value] : ContainerCollectionNameMap()) {
        if (key == name) return value;
    }
    return empty;
}

void setFurnace(FurnaceScreenController* ctrl) {
    if (gFurnace == ctrl) return;
    gFurnace = ctrl;
    if (ctrl) {
        log().info(
            "Actions: furnace screen active (tag '{}', input '{}')",
            ctrl->mRecipeTag->getString(),
            collectionName(ctrl->mIngredientEnumName)
        );
    } else {
        log().debug("Actions: furnace screen gone");
    }
}

void setSmithing(SmithingTableScreenController* ctrl) {
    if (gSmithing == ctrl) return;
    gSmithing = ctrl;
    if (ctrl) log().info("Actions: smithing table screen active");
    else log().debug("Actions: smithing table screen gone");
}

LL_TYPE_INSTANCE_HOOK(
    FurnaceTickHook,
    ll::memory::HookPriority::Normal,
    FurnaceScreenController,
    &FurnaceScreenController::$tick,
    ::ui::DirtyFlag
) {
    setFurnace(this);
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    SmithingCtorHook,
    ll::memory::HookPriority::Normal,
    SmithingTableScreenController,
    &SmithingTableScreenController::$ctor,
    void*,
    std::shared_ptr<ClientInstanceScreenModel> model,
    Player&                                    player,
    BlockPos const&                            pos,
    ActorUniqueID                              uniqueId
) {
    void* result = origin(std::move(model), player, pos, uniqueId);
    setSmithing(this);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    StationScreenDtorHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$dtor,
    void
) {
    if (gFurnace && static_cast<ContainerScreenController*>(gFurnace) == this) setFurnace(nullptr);
    if (gSmithing && static_cast<ContainerScreenController*>(gSmithing) == this) setSmithing(nullptr);
    origin();
}

// --- Container access ------------------------------------------------------------------------

struct Slot {
    std::string   collection;
    int           index = 0;
    core::ItemKey key;
    int           count    = 0;
    int           maxStack = 64;
};

/// Collections that hold the player's own items.
std::vector<std::string> inventoryCollections(ContainerManagerController& mgr) {
    auto const& combined = collectionName(ContainerEnumName::CombinedHotbarAndInventoryContainer);
    if (!combined.empty() && mgr.hasContainerController(combined)) return {combined};
    std::vector<std::string> names;
    for (auto e : {ContainerEnumName::HotbarContainer, ContainerEnumName::InventoryContainer}) {
        auto const& n = collectionName(e);
        if (!n.empty() && mgr.hasContainerController(n)) names.push_back(n);
    }
    return names;
}

bool isEmpty(ItemStack const& stack) { return stack.isNull() || stack.mCount == 0; }

void collectSlots(ContainerManagerController& mgr, std::string const& collection, std::vector<Slot>& out) {
    if (collection.empty() || !mgr.hasContainerController(collection)) return;
    int const size = mgr.getContainerSize(collection);
    for (int i = 0; i < size; ++i) {
        auto const& stack = mgr.getItemStack(collection, i);
        auto        key   = game::ItemCatalog::keyOf(stack);
        if (key.empty() || isEmpty(stack)) continue;
        out.push_back({collection, i, std::move(key), static_cast<int>(stack.mCount), stack.getMaxStackSize()});
    }
}

std::vector<Slot> inventorySlots(ContainerManagerController& mgr) {
    std::vector<Slot> slots;
    for (auto const& name : inventoryCollections(mgr)) collectSlots(mgr, name, slots);
    return slots;
}

std::vector<PoolEntry> toPool(std::vector<Slot> const& slots) {
    std::vector<PoolEntry> pool;
    for (auto const& s : slots) addToPool(pool, s.key, s.count, s.maxStack);
    return pool;
}

bool hasCursorItem(ContainerManagerController& mgr) {
    auto const& cursor = ContainerScreenController::mCursorSlotData();
    if (!mgr.hasContainerController(cursor.mCollectionName.get())) return false;
    return !isEmpty(mgr.getItemStack(cursor.mCollectionName.get(), cursor.mCollectionIndex));
}

/// Move the whole stack of a station slot back to the player's inventory: onto stacks of the same
/// item with room first, then into empty slots. True if the slot ends up empty.
bool moveToInventory(ContainerManagerController& mgr, std::string const& collection, int index) {
    auto const names = inventoryCollections(mgr);
    for (int pass = 0; pass < 2; ++pass) {
        for (auto const& inv : names) {
            int const size = mgr.getContainerSize(inv);
            for (int i = 0; i < size; ++i) {
                // Re-read every time: each move changes the (predicted) containers.
                auto const& src = mgr.getItemStack(collection, index);
                if (isEmpty(src)) return true;
                auto const& dst    = mgr.getItemStack(inv, i);
                int const   left   = src.mCount;
                int         amount = 0;
                if (pass == 0) {
                    if (isEmpty(dst) || !dst.isStackable(src)) continue;
                    amount = std::min(left, static_cast<int>(dst.getMaxStackSize()) - static_cast<int>(dst.mCount));
                } else {
                    if (!isEmpty(dst)) continue;
                    amount = left;
                }
                if (amount <= 0) continue;
                mgr.handlePlaceAmount(SlotData(collection, index), amount, SlotData(inv, i));
            }
        }
    }
    return isEmpty(mgr.getItemStack(collection, index));
}

// --- Targets ---------------------------------------------------------------------------------

/// Where a recipe goes in the open station screen.
struct Target {
    ContainerManagerController* mgr = nullptr;
    std::vector<std::string>    inputSlots; ///< Collection (slot 0) per recipe input; "" = no slot.
    std::string                 fuelSlot;   ///< Furnaces only.
};

TransferBlock blockFor(Station station) {
    switch (station) {
    case Station::Furnace:
        return TransferBlock::NoFurnace;
    case Station::BlastFurnace:
        return TransferBlock::NoBlastFurnace;
    case Station::Smoker:
        return TransferBlock::NoSmoker;
    case Station::SmithingTable:
        return TransferBlock::NoSmithingTable;
    default:
        return TransferBlock::Unsupported;
    }
}

/// The open screen matching `station`, or nullopt (wrong or no screen, no player).
std::optional<Target> targetFor(Station station, std::size_t inputs) {
    if (!game::currentPlayer()) return std::nullopt;
    Target t;
    if (!furnaceTagOf(station).empty()) {
        // A furnace only accepts its own recipes (a smoker cannot smelt ores).
        if (!gFurnace || furnaceStationOf(gFurnace->mRecipeTag->getString()) != station) return std::nullopt;
        if (!gFurnace->_isStillValid()) return std::nullopt; // block gone or out of reach
        t.mgr = gFurnace->mContainerManagerController.get();
        if (!t.mgr) return std::nullopt;
        auto const& input = collectionName(gFurnace->mIngredientEnumName);
        if (input.empty() || !t.mgr->hasContainerController(input)) return std::nullopt;
        t.inputSlots.assign(inputs, std::string{});
        if (inputs > 0) t.inputSlots[0] = input;
        auto const& fuel = collectionName(ContainerEnumName::FurnaceFuelContainer);
        if (!fuel.empty() && t.mgr->hasContainerController(fuel)) t.fuelSlot = fuel;
        return t;
    }
    if (station == Station::SmithingTable) {
        if (!gSmithing || !gSmithing->_isStillValid()) return std::nullopt;
        t.mgr = gSmithing->mContainerManagerController.get();
        if (!t.mgr) return std::nullopt;
        // RecipeView smithing inputs: template, base, addition.
        ContainerEnumName const order[] = {
            ContainerEnumName::SmithingTableTemplateContainer,
            ContainerEnumName::SmithingTableInputContainer,
            ContainerEnumName::SmithingTableMaterialContainer,
        };
        t.inputSlots.assign(inputs, std::string{});
        for (std::size_t i = 0; i < inputs && i < std::size(order); ++i) {
            auto const& name = collectionName(order[i]);
            if (!name.empty() && t.mgr->hasContainerController(name)) t.inputSlots[i] = name;
        }
        return t;
    }
    return std::nullopt;
}

/// Every input has a slot to go to (inputs beyond the station's slots, or a template input in a
/// legacy smithing screen without a template slot, make the recipe unsupported).
bool fits(core::RecipeView const& recipe, Target const& t) {
    bool any = false;
    for (std::size_t i = 0; i < recipe.inputs.size(); ++i) {
        if (recipe.inputs[i].empty()) continue;
        if (i >= t.inputSlots.size() || t.inputSlots[i].empty()) return false;
        any = true;
    }
    return any;
}

/// What the player can use for a recipe: the inventory plus the station's input slots.
std::vector<PoolEntry> availablePool(Target const& t) {
    auto slots = inventorySlots(*t.mgr);
    for (auto const& name : t.inputSlots) collectSlots(*t.mgr, name, slots);
    return toPool(slots);
}

/// All catalyst alternatives in order (the smelting providers put fuels there, coal first).
std::vector<core::ItemKey> fuelsOf(core::RecipeView const& recipe) {
    std::vector<core::ItemKey> fuels;
    for (auto const& c : recipe.catalysts) {
        for (auto const& alt : c.alternatives) {
            if (std::find(fuels.begin(), fuels.end(), alt) == fuels.end()) fuels.push_back(alt);
        }
    }
    return fuels;
}

/// Put one stack of fuel into the empty fuel slot (JEI moves fuel along with the input).
void addFuel(
    ContainerManagerController& mgr,
    Target const&               t,
    core::RecipeView const&     recipe,
    core::ItemKey const&        ingredient
) {
    if (t.fuelSlot.empty() || !isEmpty(mgr.getItemStack(t.fuelSlot, 0))) return;
    auto inventory = inventorySlots(mgr);
    auto fuel      = pickFuel(fuelsOf(recipe), toPool(inventory), ingredient);
    if (fuel.empty()) {
        log().debug("Actions: no fuel to add for {}", recipe.id);
        return;
    }
    Slot const* best = nullptr;
    for (auto const& s : inventory) {
        if (s.key == fuel && (!best || s.count > best->count)) best = &s;
    }
    if (!best) return;
    if (!mgr.handlePlaceAmount(SlotData(best->collection, best->index), best->count, SlotData(t.fuelSlot, 0))) {
        log().warn("Actions: moving fuel {} x{} failed", fuel.toString(), best->count);
        return;
    }
    log().info("Actions: added fuel {} x{}", fuel.toString(), best->count);
}

} // namespace

void install() {
    FurnaceTickHook::hook();
    SmithingCtorHook::hook();
    StationScreenDtorHook::hook();
}

void uninstall() {
    StationScreenDtorHook::unhook();
    SmithingCtorHook::unhook();
    FurnaceTickHook::unhook();
    gFurnace  = nullptr;
    gSmithing = nullptr;
}

bool handles(core::RecipeView const& recipe) {
    auto const s = stationFor(recipe.category);
    return s != Station::None && s != Station::Crafting;
}

TransferCheck check(core::RecipeView const& recipe) {
    TransferCheck result;
    auto const    station = stationFor(recipe.category);
    result.block          = blockFor(station);
    auto target           = targetFor(station, recipe.inputs.size());
    if (!target) return result;
    if (!fits(recipe, *target)) {
        result.block = TransferBlock::Unsupported;
        return result;
    }
    result.possible      = true;
    result.block         = TransferBlock::None;
    result.missingInputs = assign(recipe, availablePool(*target), 1).missing;
    return result;
}

bool transfer(core::RecipeView const& recipe, bool maxSetsWanted) {
    auto target = targetFor(stationFor(recipe.category), recipe.inputs.size());
    if (!target || !fits(recipe, *target)) return false;
    auto& mgr = *target->mgr;
    if (hasCursorItem(mgr)) {
        log().debug("Actions: transfer refused, an item is on the cursor");
        return false;
    }

    // Plan first, so nothing is touched when the transfer cannot happen. Items in the input
    // slots count as available: they are moved back to the inventory below.
    auto const pool = availablePool(*target);
    int const  sets = maxSetsWanted ? maxSets(recipe, pool) : 1;
    if (sets <= 0) return false;
    auto const plan = assign(recipe, pool, sets);
    if (!plan.complete()) return false;

    // Clear the input slots back into the inventory (like the crafting grid evacuation).
    for (auto const& name : target->inputSlots) {
        if (name.empty() || moveToInventory(mgr, name, 0)) continue;
        log().warn("Actions: {} could not be cleared (inventory full?)", name);
        return false;
    }

    auto inventory = inventorySlots(mgr);
    bool ok        = true;
    for (std::size_t i = 0; i < recipe.inputs.size() && ok; ++i) {
        auto const& dst = target->inputSlots[i];
        if (dst.empty() || plan.chosen[i].empty()) continue;
        int need = std::max(1, recipe.inputs[i].count) * sets;
        for (auto& src : inventory) {
            if (need <= 0) break;
            if (src.count <= 0 || src.key != plan.chosen[i]) continue;
            int const amount = std::min(need, src.count);
            if (!mgr.handlePlaceAmount(SlotData(src.collection, src.index), amount, SlotData(dst, 0))) {
                log().warn("Actions: moving {} x{} into {} failed", src.key.toString(), amount, dst);
                ok = false;
                break;
            }
            src.count -= amount;
            need      -= amount;
        }
        if (need > 0) ok = false;
    }
    if (ok && !target->fuelSlot.empty() && !plan.chosen.empty()) addFuel(mgr, *target, recipe, plan.chosen[0]);
    if (!ok) {
        // Don't leave half-filled slots behind.
        for (auto const& name : target->inputSlots) {
            if (!name.empty()) moveToInventory(mgr, name, 0);
        }
    }
    log().info(
        "Actions: transferred recipe {} ({} set{}) into {}: {}",
        recipe.id,
        sets,
        sets == 1 ? "" : "s",
        recipe.category,
        ok ? "ok" : "failed"
    );
    return ok;
}

} // namespace bei::actions::station
