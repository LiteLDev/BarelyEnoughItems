// Generic categories for recipe tags no dedicated provider knows about (e.g. tags added by
// add-ons or experiments), so no recipe is silently dropped.
#include "game/Game.h"
#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/other/Common.h"
#include "mod/Log.h"

#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"

#include <set>

namespace bei::game::other {

namespace {

constexpr int kFallbackOrder = 900;

std::string categoryIdFor(std::string const& tag) { return "bei:tag/" + tag; }

std::set<std::string> unhandledTags(Recipes& recipes) {
    std::set<std::string> tags;
    for (auto const& [tag, byId] : recipes.mRecipes.get()) {
        auto const& name = tag.getString();
        if (name.empty() || byId.empty() || isHandledOrIgnoredTag(name)) continue;
        tags.insert(name);
    }
    return tags;
}

class FallbackProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "fallback"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        auto* level = currentLevel();
        if (!level) return;
        auto registry = level->getItemRegistry();
        for (auto const& tag : unhandledTags(level->getRecipes())) {
            core::CategoryInfo info;
            info.id            = categoryIdFor(tag);
            info.titleKey      = "bei.category.tag." + tag;
            info.fallbackTitle = tag;
            info.layout        = core::LayoutKind::Generic;
            info.order         = kFallbackOrder;
            // Tags are usually named after their station block ("material_reducer"...).
            std::string itemName = tag.find(':') == std::string::npos ? "minecraft:" + tag : tag;
            if (auto const* item = registry.lookupByName(HashedString{itemName}).get()) {
                if (auto key = keyOf(*item, core::ItemKey::ANY_AUX); !key.empty())
                    info.workstations.push_back(std::move(key));
            }
            out.push_back(std::move(info));
        }
    }

    void collect(Level& level, Recipes& recipes, std::vector<core::RecipeView>& out) const override {
        for (auto const& tag : unhandledTags(recipes)) {
            std::size_t before   = out.size();
            std::size_t ordinal  = 0;
            auto        category = categoryIdFor(tag);
            forEachRecipeWithTag(recipes, tag, [&](std::string const& id, Recipe const& recipe) {
                ++ordinal;
                core::RecipeView view;
                view.id       = id.empty() ? recipeIdOf(recipe, tag, ordinal) : id;
                view.category = category;
                for (auto const& ingredient : recipe.mMyIngredients.get()) {
                    auto slot = toView(ingredient);
                    if (!slot.empty()) view.inputs.push_back(std::move(slot));
                }
                view.outputs = resultsOf(level, recipe);
                if (view.inputs.empty() && view.outputs.empty()) return;
                view.shapeless = recipe.isShapeless();
                view.netId     = netIdOf(recipe);
                out.push_back(std::move(view));
            });
            log().info("Recipes [{}] (unrecognized tag '{}'): {}", category, tag, out.size() - before);
        }
    }
};

} // namespace

} // namespace bei::game::other

BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::FallbackProvider);
