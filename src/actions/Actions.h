#pragma once

#include "core/model/ItemKey.h"
#include "core/model/RecipeView.h"

#include <string>
#include <vector>

class ItemStack;

namespace bei::actions {

/// Why a transfer is not possible (TransferCheck::possible == false).
enum class TransferBlock {
    None,            ///< Possible.
    Unsupported,     ///< No transfer for this category (campfire, brewing...) or recipe.
    NoGrid,          ///< Crafting recipe, but no crafting screen is open.
    TooLarge,        ///< The recipe does not fit the open grid (3x3 recipe in the 2x2 inventory).
    NoFurnace,       ///< Smelting recipe, but no furnace screen is open.
    NoBlastFurnace,  ///< Blasting recipe, but no blast furnace screen is open.
    NoSmoker,        ///< Smoking recipe, but no smoker screen is open.
    NoSmithingTable, ///< Smithing recipe, but no smithing table screen is open.
};

/// Result of checking whether a recipe can be moved into the open crafting grid.
struct TransferCheck {
    bool               possible = false; ///< A compatible crafting screen is open and the recipe fits.
    std::vector<int>   missingInputs;    ///< Indices into RecipeView::inputs the player lacks.
    TransferBlock      block = TransferBlock::Unsupported; ///< Reason when !possible.
    [[nodiscard]] bool canCraft() const noexcept { return possible && missingInputs.empty(); }
};

/// Player-affecting actions: recipe transfer ("+" button) and cheat mode.
///
/// Contract:
/// - All calls on the client thread; never touch inventories from input callbacks directly.
/// - checkTransfer/transfer support:
///   - crafting recipes in the crafting table screen, and recipes fitting 2x2 in the survival
///     inventory screen;
///   - furnace / blast furnace / smoker recipes in the screen of that exact block: the input goes
///     into the input slot, and when the fuel slot is empty one stack of a fuel from the recipe's
///     catalysts (never the item being smelted) goes into the fuel slot. Fuel is optional: a
///     missing fuel does not make the recipe "missing items";
///   - smithing recipes in the smithing table screen (template, base, addition slots).
///   Everything else is not possible (TransferBlock says why).
/// - Items already in the target slots count as available; they are moved back to the inventory
///   first.
/// - transfer(maxSets=true) moves as many sets as available (shift-click in JEI); for furnaces
///   that is up to one stack of the input.
/// - give() only works when canCheat() (operator / creative); it uses the /give command.
///
/// Implementation notes:
/// - The crafting screen is the most recent live vanilla CraftingScreenController (the inventory
///   screen and the crafting table both use it). It stays valid while another scene (e.g. the BEI
///   recipe screen) is pushed on top of it, and is forgotten when the screen is closed.
/// - Transfer goes through the vanilla container manager (the same calls as mouse clicks), so the
///   client sends normal ItemStackRequests and the server stays authoritative.
/// - give() sends a CommandRequestPacket ("/give @s <id> <count> <aux>") like the chat screen
///   does, without adding to the chat input history.
class Actions {
public:
    static Actions& get();

    void install();
    void uninstall();

    [[nodiscard]] TransferCheck checkTransfer(core::RecipeView const& recipe);
    bool                        transfer(core::RecipeView const& recipe, bool maxSets);
    /// Translated tooltip for the "+" button (move items / missing items / why not possible).
    [[nodiscard]] static std::string transferTooltip(TransferCheck const& check);

    [[nodiscard]] bool canCheat() const;
    bool               give(core::ItemKey const& item, int count);

    /// Side length of the open crafting grid: 3 (crafting table), 2 (inventory) or 0 (none).
    [[nodiscard]] int currentCraftingGridSize() const;

    /// True if the player holds an item on the cursor in the inventory / crafting table screen
    /// (other container screens are not tracked and report false).
    [[nodiscard]] bool hasCursorItem() const;
    /// The stack hasCursorItem() reports, or nullptr. Owned by the game: use it in this frame only.
    [[nodiscard]] ItemStack const* cursorItem() const;
    /// True if deleteCursorItem() can work now: cheat mode, creative mode, item on the cursor.
    /// Survival players cannot delete items: the server only accepts the destroy action in
    /// creative, and no command targets the cursor slot.
    [[nodiscard]] bool canDeleteCursorItem() const;
    /// Delete the cursor stack (overlay: click on the item panel while holding an item in cheat
    /// mode). Uses the vanilla creative "drop into the item list" action. Returns false if
    /// canDeleteCursorItem() is false.
    bool deleteCursorItem();
};

} // namespace bei::actions
