#include "game/recipes/other/Common.h"

#include "mod/Log.h"

#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/item/registry/ItemRegistry.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/actor/FurnaceBlockActor.h"

#include <algorithm>
#include <unordered_set>

namespace bei::game::other {

namespace {
/// Cook time of a regular furnace in ticks; burn durations are reported relative to it.
constexpr float kFurnaceCookTicks = 200.0f;
} // namespace

bool isHandledOrIgnoredTag(std::string_view tag) noexcept {
    return std::find(kHandledTags.begin(), kHandledTags.end(), tag) != kHandledTags.end()
        || std::find(kIgnoredTags.begin(), kIgnoredTags.end(), tag) != kIgnoredTags.end();
}

core::ItemKey keyOf(Item const& item, int aux) {
    auto const& name = item.mFullName->getString();
    if (name.empty()) return {};
    core::ItemKey key{name, static_cast<std::int16_t>(aux)};
    if (aux < 0 || aux >= core::ItemKey::ANY_AUX) key.aux = core::ItemKey::ANY_AUX;
    return key;
}

core::ItemKey keyOf(ItemStackBase const& stack) {
    if (stack.isNull()) return {};
    auto const* item = stack.mItem.get();
    if (!item) return {};
    return keyOf(*item, stack.mAuxValue);
}

std::vector<core::ItemKey> expand(ItemDescriptor const& descriptor) {
    std::vector<core::ItemKey> out;
    if (!descriptor.mImpl.get() || descriptor.isNull()) return out;
    std::unordered_set<core::ItemKey, core::ItemKeyHash> seen;
    descriptor.forEachItemUntil([&](Item const& item, short aux) {
        auto key = keyOf(item, aux);
        if (!key.empty() && seen.insert(key).second) out.push_back(std::move(key));
        return false; // keep going
    });
    if (out.empty()) {
        // Some descriptor kinds do not enumerate; fall back to the single item they name.
        if (auto const* item = descriptor.getItem()) {
            auto key = keyOf(*item, descriptor.getAuxValue());
            if (!key.empty()) out.push_back(std::move(key));
        }
    }
    return out;
}

core::IngredientView toView(RecipeIngredient const& ingredient) {
    core::IngredientView view;
    view.alternatives = expand(ingredient);
    view.count        = std::max(1, static_cast<int>(ingredient.mStackSize));
    return view;
}

core::IngredientView single(core::ItemKey key, int count) {
    core::IngredientView view;
    if (!key.empty()) view.alternatives.push_back(std::move(key));
    view.count = std::max(1, count);
    return view;
}

std::vector<core::IngredientView> resultsOf(Level& level, Recipe const& recipe) {
    std::vector<core::IngredientView> out;
    auto const&                       results = recipe.mResults.get();
    if (!results.mResultsAreLoaded) results.load(level.getBlockPalette());
    for (auto const& stack : recipe.getResultItems()) {
        auto key = keyOf(stack);
        if (key.empty()) continue;
        out.push_back(single(std::move(key), stack.mCount));
    }
    return out;
}

std::string recipeIdOf(Recipe const& recipe, std::string_view fallbackPrefix, std::size_t ordinal) {
    auto const& id = recipe.mRecipeId.get();
    if (!id.empty()) return id;
    return std::string{fallbackPrefix} + "#" + std::to_string(ordinal);
}

std::string netIdOf(Recipe const& recipe) {
    auto raw = recipe.mRecipeNetId->mRawId;
    return raw == 0 ? std::string{} : std::to_string(raw);
}

void forEachRecipeWithTag(
    Recipes&                                                      recipes,
    std::string_view                                              tag,
    std::function<void(std::string const&, Recipe const&)> const& fn
) {
    for (auto const& [hashedTag, byId] : recipes.mRecipes.get()) {
        if (hashedTag.getString() != tag) continue;
        for (auto const& [id, recipe] : byId) {
            if (!recipe) continue;
            try {
                fn(id, *recipe);
            } catch (std::exception const& e) {
                log().warn("Skipping {} recipe '{}': {}", tag, id, e.what());
            }
        }
    }
}

void forEachRegisteredItem(Level& level, std::function<void(Item const&)> const& fn) {
    auto ref      = level.getItemRegistry();
    auto registry = std::weak_ptr<ItemRegistry>{ref.mWeakRegistry}.lock();
    if (!registry) return;
    for (auto const& item : registry->mItemRegistry.get()) {
        if (auto const* ptr = item.get()) fn(*ptr);
    }
}

std::vector<FuelInfo> collectFuels(Level& level) {
    std::vector<FuelInfo>                                fuels;
    std::unordered_set<core::ItemKey, core::ItemKeyHash> seen;
    forEachRegisteredItem(level, [&](Item const& item) {
        // getItemBurnDuration only knows the hardcoded fuel table; data-driven fuels (the "fuel"
        // item component, e.g. coal and charcoal in 1.26) are only seen through a stack.
        float ticks = FurnaceBlockActor::getItemBurnDuration(item, kFurnaceCookTicks);
        if (!(ticks > 0.0f)) {
            ItemInstance stack;
            stack.reinit(item, 1, 0);
            if (!stack.isNull()) ticks = FurnaceBlockActor::getBurnDuration(stack, kFurnaceCookTicks);
        }
        if (!(ticks > 0.0f)) return;
        auto key = keyOf(item, 0);
        if (key.empty() || !seen.insert(key).second) return;
        fuels.push_back({std::move(key), static_cast<int>(ticks)});
    });
    std::sort(fuels.begin(), fuels.end(), [](FuelInfo const& a, FuelInfo const& b) {
        return a.burnTicks != b.burnTicks ? a.burnTicks > b.burnTicks : a.key < b.key;
    });
    return fuels;
}

std::vector<core::ItemKey> fuelKeys(std::vector<FuelInfo> const& fuels) {
    std::vector<core::ItemKey> keys;
    keys.reserve(fuels.size());
    for (auto const& f : fuels) keys.push_back(f.key);
    // Show the everyday fuels first when the UI cycles through the alternatives.
    std::stable_partition(keys.begin(), keys.end(), [](core::ItemKey const& k) {
        return k.id == "minecraft:coal" || k.id == "minecraft:charcoal";
    });
    return keys;
}

} // namespace bei::game::other
