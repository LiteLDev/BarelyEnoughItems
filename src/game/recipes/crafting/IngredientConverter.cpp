#include "game/recipes/crafting/IngredientConverter.h"

#include "game/items/ItemCatalog.h"

#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"

#include <algorithm>
#include <unordered_set>

namespace bei::game::crafting {

namespace {

constexpr std::string_view AIR = "minecraft:air";

/// Bedrock encodes "any aux" as -1 in some places and 0x7FFF in others.
std::int16_t normalizeAux(short aux) noexcept {
    return (aux < 0 || aux == core::ItemKey::ANY_AUX) ? core::ItemKey::ANY_AUX : static_cast<std::int16_t>(aux);
}

void addAlternative(
    core::IngredientView&                                 ing,
    std::unordered_set<core::ItemKey, core::ItemKeyHash>& seen,
    core::ItemKey                                         key
) {
    if (key.empty() || key.id == AIR) return;
    if (!seen.insert(key).second) return;
    ing.alternatives.push_back(std::move(key));
}

/// A wildcard alternative already covers concrete variants of the same item: keep only the wildcard.
void collapseWildcards(core::IngredientView& ing) {
    std::unordered_set<std::string> wildcardIds;
    for (auto const& k : ing.alternatives)
        if (k.isWildcard()) wildcardIds.insert(k.id);
    if (wildcardIds.empty()) return;
    std::erase_if(ing.alternatives, [&](core::ItemKey const& k) {
        return !k.isWildcard() && wildcardIds.contains(k.id);
    });
}

} // namespace

core::IngredientView toIngredient(ItemDescriptor const& descriptor, int count) {
    core::IngredientView ing;
    ing.count = std::max(count, 1);
    if (!descriptor.mImpl.get()) return ing;

    std::unordered_set<core::ItemKey, core::ItemKeyHash> seen;
    descriptor.forEachItemUntil([&](Item const& item, short aux) -> bool {
        auto const& name = item.mFullName.get().getString();
        if (!name.empty()) addAlternative(ing, seen, core::ItemKey{name, normalizeAux(aux)});
        return ing.alternatives.size() >= MAX_ALTERNATIVES; // true stops the iteration
    });

    // Some descriptor kinds do not enumerate; fall back to their single resolved item.
    if (ing.alternatives.empty()) {
        if (auto const* item = descriptor.getItem()) {
            auto const& name = item->mFullName.get().getString();
            if (!name.empty()) addAlternative(ing, seen, core::ItemKey{name, normalizeAux(descriptor.getAuxValue())});
        }
    }
    collapseWildcards(ing);
    return ing;
}

bool isEmptyDescriptor(ItemDescriptor const& descriptor) { return !descriptor.mImpl.get() || descriptor.isNull(); }

std::vector<core::IngredientView> toOutputs(std::vector<ItemInstance> const& results) {
    std::vector<core::IngredientView> out;
    out.reserve(results.size());
    for (auto const& inst : results) {
        auto key = ItemCatalog::keyOf(inst);
        if (key.empty() || key.id == AIR) continue;
        core::IngredientView ing;
        ing.count = std::max<int>(inst.mCount, 1);
        ing.alternatives.push_back(std::move(key));
        out.push_back(std::move(ing));
    }
    return out;
}

bool layoutShapeless(core::RecipeView& view) {
    auto n = static_cast<std::size_t>(std::ranges::count_if(view.inputs, [](auto const& ing) { return !ing.empty(); }));
    if (n > 9) return false;
    std::erase_if(view.inputs, [](core::IngredientView const& ing) { return ing.empty(); });
    int side       = n <= 1 ? 1 : (n <= 4 ? 2 : 3);
    view.shapeless = true;
    view.width     = side;
    view.height    = side;
    view.inputs.resize(static_cast<std::size_t>(side * side));
    return true;
}

namespace {
void appendIngredient(std::string& sig, core::IngredientView const& ing) {
    sig += std::to_string(ing.count);
    sig += '[';
    for (auto const& k : ing.alternatives) {
        sig += k.toString();
        sig += ',';
    }
    sig += ']';
}
} // namespace

std::string signatureOf(core::RecipeView const& view) {
    std::string sig  = view.category;
    sig             += '|';
    sig             += std::to_string(view.width) + 'x' + std::to_string(view.height) + (view.shapeless ? 's' : 'o');
    sig             += "|in:";
    if (view.shapeless) {
        // Order does not matter for shapeless recipes.
        std::vector<std::string> parts;
        for (auto const& ing : view.inputs) {
            if (ing.empty()) continue;
            std::string p;
            appendIngredient(p, ing);
            parts.push_back(std::move(p));
        }
        std::ranges::sort(parts);
        for (auto const& p : parts) sig += p;
    } else {
        for (auto const& ing : view.inputs) appendIngredient(sig, ing);
    }
    sig += "|out:";
    for (auto const& ing : view.outputs) appendIngredient(sig, ing);
    sig += "|note:" + view.note;
    return sig;
}

bool ItemLookup::exists(std::string_view fullName) const {
    auto item = mLevel.getItemRegistry().lookupByNameNoAlias(fullName);
    return item.get() != nullptr;
}

core::ItemKey ItemLookup::key(std::string_view fullName, short aux) const {
    if (!exists(fullName)) return {};
    return core::ItemKey{std::string{fullName}, normalizeAux(aux)};
}

core::IngredientView ItemLookup::anyOf(std::vector<std::string_view> const& fullNames, short aux, int count) const {
    core::IngredientView ing;
    ing.count = count;
    for (auto name : fullNames)
        if (auto k = key(name, aux); !k.empty()) ing.alternatives.push_back(std::move(k));
    return ing;
}

} // namespace bei::game::crafting
