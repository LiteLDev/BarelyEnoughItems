// Tracks the vanilla container slot under the cursor and reports its item to
// app::Controller::setVanillaHovered. Everything here runs on the client thread.
#include "input/InputState.h"

#include "app/Controller.h"
#include "game/items/ItemCatalog.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/gui/ViewRequest.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/CraftingScreenController.h"
#include "mc/world/item/ItemStackBase.h"

#include <cstdint>
#include <optional>
#include <string>

namespace bei::input::detail {

namespace {

struct HoveredSlot {
    /// Identity of the controller that reported the hover. Only compared, never dereferenced:
    /// the hooks below receive the live controller as `this`.
    std::uintptr_t               owner = 0;
    std::string                  collection;
    int                          index = -1;
    std::optional<core::ItemKey> item;
};

HoveredSlot gHovered;

std::optional<core::ItemKey> readSlot(ContainerScreenController const& controller, std::string const& name, int index) {
    // _getVisualItemStack is what vanilla uses for its own hover text; it goes through the
    // virtual _getVisualItemStackImpl, so creative/crafting/recipe-book collections work too.
    auto key = game::ItemCatalog::keyOf(controller._getVisualItemStack(name, index));
    if (key.empty()) return std::nullopt;
    return key;
}

void publish(std::optional<core::ItemKey> item) {
    if (item == gHovered.item) return;
    gHovered.item = item;
    app::Controller::get().setVanillaHovered(std::move(item));
}

void slotHovered(ContainerScreenController const& controller, std::string const& name, int index) {
    gHovered.owner      = reinterpret_cast<std::uintptr_t>(&controller);
    gHovered.collection = name;
    gHovered.index      = index;
    publish(readSlot(controller, name, index));
}

void slotUnhovered(ContainerScreenController const& controller, std::string const& name, int index) {
    // Hover/unhover of neighbouring slots may arrive in either order; only clear our slot.
    if (gHovered.owner != reinterpret_cast<std::uintptr_t>(&controller) || gHovered.index != index
        || gHovered.collection != name)
        return;
    clearSlotHover();
}

/// The slot's content changes while hovered (item picked up, crafted, ...): re-read it.
void slotScreenTicked(ContainerScreenController const& controller) {
    if (gHovered.owner != reinterpret_cast<std::uintptr_t>(&controller)) return;
    publish(readSlot(controller, gHovered.collection, gHovered.index));
}

void slotScreenLeft(ContainerScreenController const& controller) {
    if (gHovered.owner == reinterpret_cast<std::uintptr_t>(&controller)) clearSlotHover();
}

} // namespace

LL_TYPE_INSTANCE_HOOK(
    ContainerSlotHoveredHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$_onContainerSlotHovered,
    ::ui::ViewRequest,
    std::string const& collectionName,
    int                index
) {
    auto result = origin(collectionName, index);
    slotHovered(*this, collectionName, index);
    return result;
}

// The survival inventory and crafting table are CraftingScreenControllers, whose override
// does not necessarily reach the base implementation.
LL_TYPE_INSTANCE_HOOK(
    CraftingSlotHoveredHook,
    ll::memory::HookPriority::Normal,
    CraftingScreenController,
    &CraftingScreenController::$_onContainerSlotHovered,
    ::ui::ViewRequest,
    std::string const& collectionName,
    int                index
) {
    auto result = origin(collectionName, index);
    slotHovered(*this, collectionName, index);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ContainerSlotUnhoveredHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$_onContainerSlotUnhovered,
    ::ui::ViewRequest,
    std::string const& collectionName,
    int                index
) {
    auto result = origin(collectionName, index);
    slotUnhovered(*this, collectionName, index);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ContainerTickHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$tick,
    ::ui::DirtyFlag
) {
    auto result = origin();
    slotScreenTicked(*this);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    CraftingTickHook,
    ll::memory::HookPriority::Normal,
    CraftingScreenController,
    &CraftingScreenController::$tick,
    ::ui::DirtyFlag
) {
    auto result = origin();
    slotScreenTicked(*this);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ContainerLeaveHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$onLeave,
    void
) {
    slotScreenLeft(*this);
    origin();
}

void clearSlotHover() {
    gHovered = {};
    app::Controller::get().setVanillaHovered(std::nullopt);
}

void installSlotHover() {
    ContainerSlotHoveredHook::hook();
    CraftingSlotHoveredHook::hook();
    ContainerSlotUnhoveredHook::hook();
    ContainerTickHook::hook();
    CraftingTickHook::hook();
    ContainerLeaveHook::hook();
}

void uninstallSlotHover() {
    ContainerLeaveHook::unhook();
    CraftingTickHook::unhook();
    ContainerTickHook::unhook();
    ContainerSlotUnhoveredHook::unhook();
    CraftingSlotHoveredHook::unhook();
    ContainerSlotHoveredHook::unhook();
    clearSlotHover();
}

} // namespace bei::input::detail
