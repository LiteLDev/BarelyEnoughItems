// Anvil repair category (JEI's "Anvil" category, repair part).
//
// The game has no anvil recipe data, so we ask it which items repair which:
// - data-driven items (Item::isComponentBased) list their repair materials in the "repairable"
//   item component (RepairableItemComponent::mRepairItems), read directly;
// - the remaining hardcoded item classes (tools, weapons, armor, shield...) answer through
//   Item::isValidRepairItem, asked for every registered non-damageable item (a few ms). Some
//   classes cannot answer on the client (MaceItem faults): known ones use a hardcoded material
//   (kKnownRepairs), and every call is guarded so an unknown one only loses its materials.
//
// For each damageable item:
// - "<item> + <material> -> <item>" when at least one repair material exists;
// - "<item> + <item> -> <item>": combining two damaged items of the same kind (any damageable item).
// Item keys carry no damage, so the icons show undamaged items; the notes say what happens.
#include "game/Game.h"
#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/other/Common.h"
#include "mod/Log.h"

#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/util/BaseGameVersion.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/components/RepairItemEntry.h"
#include "mc/world/item/components/RepairableItemComponent.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"

#include <algorithm>
#include <chrono>
#include <memory>

namespace bei::game::other {

namespace {

/// Anvil damage states. Bedrock lists them as separate items (older versions used aux values of
/// minecraft:anvil); only the ones present in the registry are shown.
constexpr char const* kAnvils[] = {"minecraft:anvil", "minecraft:chipped_anvil", "minecraft:damaged_anvil"};

struct Entry {
    Item const*                   item = nullptr;
    core::ItemKey                 key;
    std::unique_ptr<ItemInstance> stack; ///< Heap-allocated: ItemInstance is not cheap to move.
    bool                          damageable = false;
};

/// Hardcoded item classes whose isValidRepairItem cannot be asked on the client: MaceItem's
/// dereferences a null pointer there (crash seen in testing). Their vanilla repair materials.
struct KnownRepair {
    char const* item;
    char const* material;
};
constexpr KnownRepair kKnownRepairs[] = {
    {"minecraft:mace", "minecraft:breeze_rod"},
};

KnownRepair const* knownRepairOf(std::string_view id) {
    for (auto const& k : kKnownRepairs) {
        if (id == k.item) return &k;
    }
    return nullptr;
}

/// Item::isValidRepairItem behind a structured exception guard: some hardcoded item classes read
/// server-only state. A fault is reported through `faulted` (no C++ objects live in this frame,
/// so nothing needs unwinding).
bool guardedIsValidRepairItem(
    Item const&            item,
    ItemStackBase const&   source,
    ItemStackBase const&   repair,
    BaseGameVersion const& version,
    bool&                  faulted
) {
    __try {
        return item.isValidRepairItem(source, repair, version);
    } __except (1 /* EXCEPTION_EXECUTE_HANDLER */) {
        faulted = true;
        return false;
    }
}

void addMaterial(core::IngredientView& materials, core::ItemKey key, Entry const& target) {
    if (key.empty() || key.id == target.key.id) return; // the item itself is the "combine" recipe
    auto& alts = materials.alternatives;
    if (std::find(alts.begin(), alts.end(), key) == alts.end()) alts.push_back(std::move(key));
}

class AnvilProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "anvil"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        core::CategoryInfo info;
        info.id            = core::category::ANVIL;
        info.titleKey      = "bei.category.anvil";
        info.fallbackTitle = "Anvil";
        info.layout        = core::LayoutKind::Generic;
        info.order         = 45;
        auto* level        = currentLevel();
        for (auto const* id : kAnvils) {
            if (level && !level->getItemRegistry().lookupByName(HashedString{id}).get()) continue;
            info.workstations.push_back(core::ItemKey{id, core::ItemKey::ANY_AUX});
        }
        out.push_back(std::move(info));
    }

    void collect(Level& level, Recipes&, std::vector<core::RecipeView>& out) const override {
        auto const start = std::chrono::steady_clock::now();

        std::vector<Entry> all;
        forEachRegisteredItem(level, [&](Item const& item) {
            auto key = keyOf(item, 0);
            if (key.empty()) return;
            auto stack = std::make_unique<ItemInstance>();
            stack->reinit(item, 1, 0);
            if (stack->isNull()) return;
            bool const damageable = item.isDamageable() && item.getMaxDamage() > 0;
            all.push_back({&item, std::move(key), std::move(stack), damageable});
        });

        auto const& version      = level.getLevelData().getBaseGameVersion();
        auto const& repairableId = RepairableItemComponent::getIdentifier();
        std::size_t before = out.size(), damageable = 0, withMaterial = 0, dataDriven = 0;
        for (auto const& target : all) {
            if (!target.damageable) continue;
            ++damageable;
            core::IngredientView materials;
            if (target.item->isComponentBased()) {
                ++dataDriven;
                auto const* repairable =
                    static_cast<RepairableItemComponent const*>(target.item->getComponent(repairableId));
                if (repairable) {
                    for (auto const& entry : repairable->mRepairItems.get()) {
                        for (auto const& descriptor : entry.mItems.get()) {
                            for (auto& key : expand(descriptor)) addMaterial(materials, std::move(key), target);
                        }
                    }
                }
            } else if (auto const* known = knownRepairOf(target.key.id)) {
                addMaterial(materials, core::ItemKey{known->material, 0}, target);
            } else {
                // Repair materials are never damageable themselves (except the item itself).
                bool faulted = false;
                for (auto const& candidate : all) {
                    if (candidate.damageable) continue;
                    if (guardedIsValidRepairItem(*target.item, *target.stack, *candidate.stack, version, faulted)) {
                        addMaterial(materials, candidate.key, target);
                    }
                    if (faulted) break;
                }
                if (faulted) {
                    log().warn("Anvil: the game cannot list repair materials of {} on the client", target.key.toString());
                    materials.alternatives.clear();
                }
            }
            if (!materials.empty()) {
                ++withMaterial;
                core::RecipeView view;
                view.id       = "bei:anvil/repair/" + target.key.toString();
                view.category = core::category::ANVIL;
                view.inputs.push_back(single(target.key));
                view.inputs.push_back(std::move(materials));
                view.outputs.push_back(single(target.key));
                view.note = "bei.anvil.repair_material";
                out.push_back(std::move(view));
            }
            core::RecipeView combine;
            combine.id       = "bei:anvil/combine/" + target.key.toString();
            combine.category = core::category::ANVIL;
            combine.inputs.push_back(single(target.key));
            combine.inputs.push_back(single(target.key));
            combine.outputs.push_back(single(target.key));
            combine.note = "bei.anvil.combine";
            out.push_back(std::move(combine));
        }
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        log().info(
            "Recipes [{}]: {} ({} damageable items, {} data-driven, {} with repair materials, {} items scanned, {} ms)",
            core::category::ANVIL,
            out.size() - before,
            damageable,
            dataDriven,
            withMaterial,
            all.size(),
            ms.count()
        );
    }
};

} // namespace

} // namespace bei::game::other

BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::AnvilProvider);
