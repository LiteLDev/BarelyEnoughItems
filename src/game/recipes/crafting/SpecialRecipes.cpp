#include "game/recipes/crafting/SpecialRecipes.h"

#include "game/recipes/crafting/IngredientConverter.h"

#include "mc/world/item/crafting/BannerAddPatternRecipe.h"
#include "mc/world/item/crafting/BannerDuplicateRecipe.h"
#include "mc/world/item/crafting/BookCloningRecipe.h"
#include "mc/world/item/crafting/DecoratedPotRecipe.h"
#include "mc/world/item/crafting/FireworksRecipe.h"
#include "mc/world/item/crafting/MapCloningRecipe.h"
#include "mc/world/item/crafting/MapExtendingRecipe.h"
#include "mc/world/item/crafting/MapLockingRecipe.h"
#include "mc/world/item/crafting/MapUpgradingRecipe.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RepairItemRecipe.h"
#include "mc/world/item/crafting/ShapedChemistryRecipe.h"
#include "mc/world/item/crafting/ShapedRecipe.h"
#include "mc/world/item/crafting/ShapelessRecipe.h"
#include "mc/world/item/crafting/ShieldRecipe.h"
#include "mc/world/item/crafting/UserDataShapelessRecipe.h"

#include <array>
#include <utility>

namespace bei::game::crafting {

namespace {

using core::IngredientView;
using core::ItemKey;
using core::RecipeView;

struct KindVtable {
    void** (*vftable)();
    RecipeKind kind;
};

// Exact-type detection: game classes have no RTTI we can dynamic_cast to, but each concrete
// class exports its vftable. Matching is exact, so a subclass never matches its base entry.
KindVtable const KNOWN_VTABLES[]{
    KindVtable{&ShapedChemistryRecipe::$vftable,   RecipeKind::ShapedChemistry  },
    KindVtable{&UserDataShapelessRecipe::$vftable, RecipeKind::UserDataShapeless},
    KindVtable{&ShapedRecipe::$vftable,            RecipeKind::Shaped           },
    KindVtable{&ShapelessRecipe::$vftable,         RecipeKind::Shapeless        },
    KindVtable{&MapCloningRecipe::$vftable,        RecipeKind::MapCloning       },
    KindVtable{&MapExtendingRecipe::$vftable,      RecipeKind::MapExtending     },
    KindVtable{&MapUpgradingRecipe::$vftable,      RecipeKind::MapUpgrading     },
    KindVtable{&MapLockingRecipe::$vftable,        RecipeKind::MapLocking       },
    KindVtable{&BookCloningRecipe::$vftable,       RecipeKind::BookCloning      },
    KindVtable{&BannerDuplicateRecipe::$vftable,   RecipeKind::BannerDuplicate  },
    KindVtable{&BannerAddPatternRecipe::$vftable,  RecipeKind::BannerAddPattern },
    KindVtable{&FireworksRecipe::$vftable,         RecipeKind::Fireworks        },
    KindVtable{&DecoratedPotRecipe::$vftable,      RecipeKind::DecoratedPot     },
    KindVtable{&ShieldRecipe::$vftable,            RecipeKind::Shield           },
    KindVtable{&RepairItemRecipe::$vftable,        RecipeKind::RepairItem       },
};

constexpr std::array<std::string_view, 20> DYES{
    "minecraft:white_dye",
    "minecraft:orange_dye",
    "minecraft:magenta_dye",
    "minecraft:light_blue_dye",
    "minecraft:yellow_dye",
    "minecraft:lime_dye",
    "minecraft:pink_dye",
    "minecraft:gray_dye",
    "minecraft:light_gray_dye",
    "minecraft:cyan_dye",
    "minecraft:purple_dye",
    "minecraft:blue_dye",
    "minecraft:brown_dye",
    "minecraft:green_dye",
    "minecraft:red_dye",
    "minecraft:black_dye",
    // Legacy dye items the game still accepts as dyes.
    "minecraft:ink_sac",
    "minecraft:bone_meal",
    "minecraft:cocoa_beans",
    "minecraft:lapis_lazuli",
};

constexpr std::array<std::string_view, 24> POT_FACES{
    "minecraft:brick",
    "minecraft:angler_pottery_sherd",
    "minecraft:archer_pottery_sherd",
    "minecraft:arms_up_pottery_sherd",
    "minecraft:blade_pottery_sherd",
    "minecraft:brewer_pottery_sherd",
    "minecraft:burn_pottery_sherd",
    "minecraft:danger_pottery_sherd",
    "minecraft:explorer_pottery_sherd",
    "minecraft:flow_pottery_sherd",
    "minecraft:friend_pottery_sherd",
    "minecraft:guster_pottery_sherd",
    "minecraft:heart_pottery_sherd",
    "minecraft:heartbreak_pottery_sherd",
    "minecraft:howl_pottery_sherd",
    "minecraft:miner_pottery_sherd",
    "minecraft:mourner_pottery_sherd",
    "minecraft:plenty_pottery_sherd",
    "minecraft:prize_pottery_sherd",
    "minecraft:scrape_pottery_sherd",
    "minecraft:sheaf_pottery_sherd",
    "minecraft:shelter_pottery_sherd",
    "minecraft:skull_pottery_sherd",
    "minecraft:snort_pottery_sherd",
};

template <std::size_t N>
std::vector<std::string_view> names(std::array<std::string_view, N> const& arr) {
    return {arr.begin(), arr.end()};
}

class Builder {
public:
    Builder(RecipeKind kind, ItemLookup const& items, std::vector<RecipeView>& out)
    : mKind(kind),
      mItems(items),
      mOut(out) {}

    [[nodiscard]] IngredientView item(std::string_view name, short aux = 0, int count = 1) const {
        return mItems.anyOf({name}, aux, count);
    }
    [[nodiscard]] IngredientView any(std::string_view name, int count = 1) const {
        return mItems.anyOf({name}, ItemKey::ANY_AUX, count);
    }
    [[nodiscard]] IngredientView anyOf(std::vector<std::string_view> const& list) const { return mItems.anyOf(list); }

    /// Shapeless example. Skipped when any listed ingredient is missing in this world.
    void shapeless(std::vector<IngredientView> inputs, IngredientView output, char const* noteKey) {
        RecipeView view = base(std::move(output), noteKey);
        if (!view.outputs.empty() && allPresent(inputs)) {
            view.inputs = std::move(inputs);
            if (layoutShapeless(view)) push(std::move(view));
        }
    }

    /// Shaped 3x3 example; `cells` are row-major, nullopt-like empty ingredients mean empty slots.
    void shaped3x3(std::vector<IngredientView> cells, IngredientView output, char const* noteKey) {
        RecipeView view = base(std::move(output), noteKey);
        bool       any  = false;
        for (auto const& c : cells) any |= !c.empty();
        if (view.outputs.empty() || !any || cells.size() != 9) return;
        view.width  = 3;
        view.height = 3;
        view.inputs = std::move(cells);
        push(std::move(view));
    }

private:
    RecipeView base(IngredientView output, char const* noteKey) const {
        RecipeView view;
        view.category = core::category::CRAFTING;
        view.note     = noteKey;
        // No netId: these are illustrations, not the real (input-dependent) game recipe.
        if (!output.empty()) view.outputs.push_back(std::move(output));
        return view;
    }
    static bool allPresent(std::vector<IngredientView> const& inputs) {
        for (auto const& i : inputs)
            if (i.empty()) return false;
        return !inputs.empty();
    }
    void push(RecipeView view) {
        view.id = "bei:special/" + std::string{kindName(mKind)} + "/" + std::to_string(mCount++);
        mOut.push_back(std::move(view));
    }

    RecipeKind               mKind;
    ItemLookup const&        mItems;
    std::vector<RecipeView>& mOut;
    int                      mCount = 0;
};

} // namespace

RecipeKind classify(Recipe const& recipe) {
    void* const vtable = *reinterpret_cast<void* const*>(&recipe);
    for (auto const& entry : KNOWN_VTABLES)
        if (vtable == static_cast<void*>(entry.vftable())) return entry.kind;
    if (recipe.isMultiRecipe()) return RecipeKind::UnknownMulti;
    return recipe.isShapeless() ? RecipeKind::UnknownShapeless : RecipeKind::UnknownShaped;
}

std::string_view kindName(RecipeKind kind) {
    switch (kind) {
    case RecipeKind::Shaped:
        return "shaped";
    case RecipeKind::Shapeless:
        return "shapeless";
    case RecipeKind::ShapedChemistry:
        return "shaped_chemistry";
    case RecipeKind::UserDataShapeless:
        return "user_data_shapeless";
    case RecipeKind::MapCloning:
        return "map_cloning";
    case RecipeKind::MapExtending:
        return "map_extending";
    case RecipeKind::MapUpgrading:
        return "map_upgrading";
    case RecipeKind::MapLocking:
        return "map_locking";
    case RecipeKind::BookCloning:
        return "book_cloning";
    case RecipeKind::BannerDuplicate:
        return "banner_duplicate";
    case RecipeKind::BannerAddPattern:
        return "banner_add_pattern";
    case RecipeKind::Fireworks:
        return "fireworks";
    case RecipeKind::DecoratedPot:
        return "decorated_pot";
    case RecipeKind::Shield:
        return "shield";
    case RecipeKind::RepairItem:
        return "repair_item";
    case RecipeKind::UnknownMulti:
        return "unknown_multi";
    case RecipeKind::UnknownShaped:
        return "unknown_shaped";
    case RecipeKind::UnknownShapeless:
        return "unknown_shapeless";
    }
    return "unknown";
}

bool isSpecial(RecipeKind kind) {
    switch (kind) {
    case RecipeKind::Shaped:
    case RecipeKind::Shapeless:
    case RecipeKind::ShapedChemistry:
    case RecipeKind::UserDataShapeless:
    case RecipeKind::UnknownShaped:
    case RecipeKind::UnknownShapeless:
        return false;
    default:
        return true;
    }
}

void buildSpecialExamples(RecipeKind kind, ItemLookup const& items, std::vector<RecipeView>& out) {
    Builder b{kind, items, out};
    switch (kind) {
    case RecipeKind::MapCloning:
        b.shapeless(
            {b.any("minecraft:filled_map"), b.any("minecraft:empty_map")},
            b.any("minecraft:filled_map", 2),
            "bei.crafting.note.map_cloning"
        );
        break;
    case RecipeKind::MapExtending: {
        auto paper = b.item("minecraft:paper");
        b.shaped3x3(
            {paper, paper, paper, paper, b.any("minecraft:filled_map"), paper, paper, paper, paper},
            b.any("minecraft:filled_map"),
            "bei.crafting.note.map_extending"
        );
        break;
    }
    case RecipeKind::MapUpgrading:
        b.shapeless(
            {b.any("minecraft:filled_map"), b.item("minecraft:compass")},
            b.any("minecraft:filled_map"),
            "bei.crafting.note.map_upgrading"
        );
        break;
    case RecipeKind::MapLocking:
        b.shapeless(
            {b.any("minecraft:filled_map"), b.item("minecraft:glass_pane")},
            b.any("minecraft:filled_map"),
            "bei.crafting.note.map_locking"
        );
        break;
    case RecipeKind::BookCloning:
        b.shapeless(
            {b.any("minecraft:written_book"), b.item("minecraft:writable_book")},
            b.any("minecraft:written_book", 2),
            "bei.crafting.note.book_cloning"
        );
        break;
    case RecipeKind::BannerDuplicate:
        b.shapeless(
            {b.any("minecraft:banner"), b.any("minecraft:banner")},
            b.any("minecraft:banner", 2),
            "bei.crafting.note.banner_duplicate"
        );
        break;
    case RecipeKind::BannerAddPattern:
        b.shapeless(
            {b.any("minecraft:banner"), b.anyOf(names(DYES))},
            b.any("minecraft:banner"),
            "bei.crafting.note.banner_add_pattern"
        );
        break;
    case RecipeKind::Fireworks:
        b.shapeless(
            {b.item("minecraft:gunpowder"), b.anyOf(names(DYES))},
            b.any("minecraft:firework_star"),
            "bei.crafting.note.firework_star"
        );
        b.shapeless(
            {b.item("minecraft:paper"), b.item("minecraft:gunpowder"), b.any("minecraft:firework_star")},
            b.item("minecraft:firework_rocket", 0, 3),
            "bei.crafting.note.firework_rocket"
        );
        break;
    case RecipeKind::DecoratedPot: {
        auto face = b.anyOf(names(POT_FACES));
        b.shaped3x3(
            {{}, face, {}, face, {}, face, {}, face, {}},
            b.item("minecraft:decorated_pot"),
            "bei.crafting.note.decorated_pot"
        );
        break;
    }
    case RecipeKind::Shield:
        b.shapeless(
            {b.item("minecraft:shield"), b.any("minecraft:banner")},
            b.item("minecraft:shield"),
            "bei.crafting.note.shield"
        );
        break;
    default:
        // RepairItem (any two damaged copies) and unknown multi recipes have no fixed display.
        break;
    }
}

} // namespace bei::game::crafting
