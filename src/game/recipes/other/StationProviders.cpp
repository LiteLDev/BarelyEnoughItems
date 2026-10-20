// Stonecutter, smithing table and cartography table recipes.
#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/other/Common.h"
#include "mod/Log.h"

#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/level/Level.h"

namespace bei::game::other {

namespace {

core::CategoryInfo makeCategory(
    char const*      id,
    char const*      titleKey,
    char const*      fallbackTitle,
    core::LayoutKind layout,
    char const*      workstation,
    int              order
) {
    core::CategoryInfo info;
    info.id            = id;
    info.titleKey      = titleKey;
    info.fallbackTitle = fallbackTitle;
    info.layout        = layout;
    info.workstations  = {
        core::ItemKey{workstation, 0}
    };
    info.order = order;
    return info;
}

class StonecutterProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "stonecutter"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        out.push_back(makeCategory(
            core::category::STONECUTTER,
            "bei.category.stonecutter",
            "Stonecutting",
            core::LayoutKind::SingleInput,
            "minecraft:stonecutter_block",
            20
        ));
    }

    void collect(Level& level, Recipes& recipes, std::vector<core::RecipeView>& out) const override {
        std::size_t before  = out.size();
        std::size_t ordinal = 0;
        forEachRecipeWithTag(recipes, "stonecutter", [&](std::string const& id, Recipe const& recipe) {
            ++ordinal;
            auto const& ingredients = recipe.mMyIngredients.get();
            if (ingredients.empty()) return;
            core::RecipeView view;
            view.id       = id.empty() ? recipeIdOf(recipe, "stonecutter", ordinal) : id;
            view.category = core::category::STONECUTTER;
            view.inputs.push_back(toView(ingredients.front()));
            view.outputs = resultsOf(level, recipe);
            if (view.inputs.front().empty() || view.outputs.empty()) return;
            view.netId = netIdOf(recipe);
            out.push_back(std::move(view));
        });
        log().info("Recipes [{}]: {}", core::category::STONECUTTER, out.size() - before);
    }
};

class SmithingProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "smithing"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        out.push_back(makeCategory(
            core::category::SMITHING,
            "bei.category.smithing",
            "Smithing",
            core::LayoutKind::Smithing,
            "minecraft:smithing_table",
            30
        ));
    }

    void collect(Level& level, Recipes& recipes, std::vector<core::RecipeView>& out) const override {
        std::size_t before  = out.size();
        std::size_t ordinal = 0;
        int         trims   = 0;
        forEachRecipeWithTag(recipes, "smithing_table", [&](std::string const& id, Recipe const& recipe) {
            ++ordinal;
            auto const& ingredients = recipe.mMyIngredients.get();
            // SmithingTransformRecipe / SmithingTrimRecipe: template, base, addition.
            if (ingredients.size() < 3) return;
            core::RecipeView view;
            view.id       = id.empty() ? recipeIdOf(recipe, "smithing", ordinal) : id;
            view.category = core::category::SMITHING;
            for (std::size_t i = 0; i < 3; ++i) view.inputs.push_back(toView(ingredients[i]));
            if (view.inputs[1].empty()) return; // no base item: nothing to show
            // Transform recipes (netherite upgrades) carry their result. Trim recipes have none (the
            // result is computed when crafting): the output is the base item with a pattern added.
            // Note: hasDataDrivenResult() does not distinguish them reliably on the client.
            view.outputs = resultsOf(level, recipe);
            if (view.outputs.empty()) {
                view.outputs.push_back(view.inputs[1]);
                view.outputs.back().count = 1;
                view.note                 = "bei.recipe.smithing_trim";
                ++trims;
            }
            view.netId = netIdOf(recipe);
            out.push_back(std::move(view));
        });
        log().info("Recipes [{}]: {} ({} trims)", core::category::SMITHING, out.size() - before, trims);
    }
};

/// The cartography table's hardcoded map recipes (game ids of their MultiRecipe classes).
struct SpecialMapRecipe {
    char const* id;
    char const* addition;
    int         outputCount;
    char const* noteKey;
};

constexpr SpecialMapRecipe kSpecialMapRecipes[] = {
    {"MapCloningRecipe",   "minecraft:empty_map",  2, "bei.recipe.map_cloning"  },
    {"MapExtendingRecipe", "minecraft:paper",      1, "bei.recipe.map_extending"},
    {"MapLockingRecipe",   "minecraft:glass_pane", 1, "bei.recipe.map_locking"  },
    {"MapUpgradingRecipe", "minecraft:compass",    1, "bei.recipe.map_upgrading"},
};

SpecialMapRecipe const* findSpecialMapRecipe(std::string_view id) {
    for (auto const& r : kSpecialMapRecipes) {
        if (id == r.id) return &r;
    }
    return nullptr;
}

class CartographyProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "cartography"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        out.push_back(makeCategory(
            core::category::CARTOGRAPHY,
            "bei.category.cartography",
            "Cartography",
            core::LayoutKind::Generic,
            "minecraft:cartography_table",
            50
        ));
    }

    void collect(Level& level, Recipes& recipes, std::vector<core::RecipeView>& out) const override {
        std::size_t before  = out.size();
        std::size_t ordinal = 0;
        forEachRecipeWithTag(recipes, "cartography_table", [&](std::string const& id, Recipe const& recipe) {
            ++ordinal;
            core::RecipeView view;
            view.id       = id.empty() ? recipeIdOf(recipe, "cartography", ordinal) : id;
            view.category = core::category::CARTOGRAPHY;
            if (auto const* special = findSpecialMapRecipe(view.id)) {
                // Hardcoded multi-recipes have no ingredient data; show what they do.
                view.inputs.push_back(single(core::ItemKey{"minecraft:filled_map", 0}));
                view.inputs.push_back(single(core::ItemKey{special->addition, 0}));
                view.outputs.push_back(single(core::ItemKey{"minecraft:filled_map", 0}, special->outputCount));
                view.note = special->noteKey;
                out.push_back(std::move(view));
                return;
            }
            for (auto const& ingredient : recipe.mMyIngredients.get()) {
                auto slot = toView(ingredient);
                if (!slot.empty()) view.inputs.push_back(std::move(slot));
            }
            view.outputs = resultsOf(level, recipe);
            if (view.inputs.empty() || view.outputs.empty()) {
                log().debug("Cartography recipe '{}' has no ingredient/result data; skipped", view.id);
                return;
            }
            view.netId = netIdOf(recipe);
            out.push_back(std::move(view));
        });
        log().info("Recipes [{}]: {}", core::category::CARTOGRAPHY, out.size() - before);
    }
};

} // namespace

} // namespace bei::game::other

BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::StonecutterProvider);
BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::SmithingProvider);
BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::CartographyProvider);
