// Crafting-table recipes (shaped, shapeless, chemistry, user-data and "special" multi recipes).

#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/crafting/IngredientConverter.h"
#include "game/recipes/crafting/SpecialRecipes.h"
#include "mod/Log.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/platform/UUID.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/level/Level.h"

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace bei::game {

namespace {

using crafting::RecipeKind;

constexpr std::string_view CRAFTING_TABLE_TAG = "crafting_table";
constexpr int              MAX_GRID           = 3;

struct Stats {
    std::map<std::string_view, int> perKind; // converted recipes per kind
    int                             duplicates = 0;
    int                             broken     = 0;
    int                             skipped    = 0; // no meaningful display
};

/// Recipe results; loads network-delivered results first (the client keeps them unresolved until needed).
std::vector<ItemInstance> const& resultsOf(Level& level, Recipe const& recipe) {
    auto const& results = recipe.mResults.get();
    if (!results.mResultsAreLoaded) results.load(level.getBlockPalette());
    return recipe.getResultItems();
}

std::string recipeId(Recipe const& recipe, std::string const& mapKey) {
    std::string const& id = recipe.mRecipeId.get();
    if (!id.empty()) return id;
    if (!mapKey.empty()) return mapKey;
    return recipe.getId().asString();
}

/// Converts a shaped grid. Returns false when the recipe has an unusable shape, no inputs, or an
/// ingredient that resolves to no item in this world (showing it would display an uncraftable recipe).
bool convertShaped(Recipe const& recipe, core::RecipeView& view) {
    int const w = recipe.mWidth;
    int const h = recipe.mHeight;
    if (w < 1 || h < 1 || w > MAX_GRID || h > MAX_GRID) return false;
    // getIngredient() indexes mMyIngredients; make sure an odd subclass cannot read past its end.
    if (recipe.mMyIngredients.get().size() < static_cast<std::size_t>(w * h)) return false;
    view.width  = w;
    view.height = h;
    view.inputs.reserve(static_cast<std::size_t>(w * h));
    bool any = false;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            RecipeIngredient const& ing      = recipe.getIngredient(x, y);
            auto                    view_ing = crafting::toIngredient(ing, ing.mStackSize);
            if (view_ing.empty() && !crafting::isEmptyDescriptor(ing)) return false;
            any |= !view_ing.empty();
            view.inputs.push_back(std::move(view_ing));
        }
    }
    return any;
}

/// Converts a shapeless ingredient list. An ingredient with stack size N occupies N grid slots
/// (that is how the game matches it) as long as everything fits a 3x3 grid.
bool convertShapeless(Recipe const& recipe, core::RecipeView& view) {
    auto const& ingredients = recipe.mMyIngredients.get();
    int         slots       = 0;
    for (auto const& ing : ingredients) slots += std::max<int>(ing.mStackSize, 1);
    bool const spread = slots <= MAX_GRID * MAX_GRID;
    for (auto const& ing : ingredients) {
        int  n        = std::max<int>(ing.mStackSize, 1);
        auto view_ing = crafting::toIngredient(ing, spread ? 1 : n);
        if (view_ing.empty()) {
            if (!crafting::isEmptyDescriptor(ing)) return false; // unresolvable ingredient
            continue;
        }
        for (int i = 0, copies = spread ? n : 1; i < copies; ++i) view.inputs.push_back(view_ing);
    }
    if (view.inputs.empty()) return false;
    return crafting::layoutShapeless(view);
}

class CraftingRecipeProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "crafting"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        core::CategoryInfo info;
        info.id            = core::category::CRAFTING;
        info.titleKey      = "bei.category.crafting";
        info.fallbackTitle = "Crafting";
        info.layout        = core::LayoutKind::CraftingGrid;
        info.workstations  = {
            core::ItemKey{"minecraft:crafting_table", 0},
            core::ItemKey{"minecraft:crafter",        0}
        };
        info.order = 0;
        out.push_back(std::move(info));
    }

    void collect(Level& level, Recipes& recipes, std::vector<core::RecipeView>& out) const override {
        auto const& byTag = recipes.mRecipes.get();
        auto        it    = byTag.find(HashedString{CRAFTING_TABLE_TAG});
        if (it == byTag.end()) {
            log().debug("Crafting: no '{}' recipes in this world", CRAFTING_TABLE_TAG);
            return;
        }

        crafting::ItemLookup            items{level};
        Stats                           stats;
        std::unordered_set<std::string> ids;
        std::unordered_set<RecipeKind>  specialsDone;
        std::size_t const               firstOut = out.size();
        // Content hash -> indices into `out`; full signatures are only recomputed on hash hits.
        std::unordered_map<std::size_t, std::vector<std::size_t>> byHash;

        auto emit = [&](core::RecipeView&& view) {
            std::string sig    = crafting::signatureOf(view);
            auto&       bucket = byHash[std::hash<std::string>{}(sig)];
            for (auto idx : bucket) {
                if (crafting::signatureOf(out[idx]) == sig) {
                    ++stats.duplicates;
                    return false;
                }
            }
            bucket.push_back(out.size());
            // Ids must be unique within the category; disambiguate (rare) collisions.
            if (!ids.insert(view.id).second) {
                std::string base = view.id;
                for (int n = 2; !ids.insert(view.id = base + "#" + std::to_string(n)).second; ++n) {}
            }
            out.push_back(std::move(view));
            return true;
        };

        for (auto const& [key, recipePtr] : it->second) {
            Recipe const* recipe = recipePtr.get();
            if (!recipe) {
                ++stats.broken;
                continue;
            }
            try {
                RecipeKind kind = crafting::classify(*recipe);

                if (crafting::isSpecial(kind)) {
                    // One illustrative set of examples per special kind, even if the game registers it twice.
                    if (!specialsDone.insert(kind).second) continue;
                    std::vector<core::RecipeView> examples;
                    crafting::buildSpecialExamples(kind, items, examples);
                    int emitted = 0;
                    for (auto& ex : examples) emitted += emit(std::move(ex)) ? 1 : 0;
                    if (emitted == 0) {
                        ++stats.skipped;
                        log().debug("Crafting: no display for special recipe '{}' ({})", key, crafting::kindName(kind));
                        continue;
                    }
                    stats.perKind[crafting::kindName(kind)] += emitted;
                    continue;
                }

                core::RecipeView view;
                view.id       = recipeId(*recipe, key);
                view.category = core::category::CRAFTING;
                view.netId    = std::to_string(recipe->mRecipeNetId.get().mRawId);
                view.outputs  = crafting::toOutputs(resultsOf(level, *recipe));
                if (view.outputs.empty()) {
                    ++stats.broken;
                    log().debug("Crafting: recipe '{}' has no valid result, skipped", view.id);
                    continue;
                }

                bool ok = recipe->isShapeless() ? convertShapeless(*recipe, view) : convertShaped(*recipe, view);
                if (!ok) {
                    ++stats.broken;
                    log().debug("Crafting: recipe '{}' has no displayable inputs, skipped", view.id);
                    continue;
                }
                if (emit(std::move(view))) ++stats.perKind[crafting::kindName(kind)];
            } catch (...) {
                // C++ exceptions only (e.g. bad_alloc, std::length_error from odd data). Memory faults are
                // avoided by the shape/bounds checks above, not caught here.
                ++stats.broken;
                log().debug("Crafting: recipe '{}' failed to convert, skipped", key);
            }
        }

        std::string kinds;
        for (auto const& [kind, n] : stats.perKind) kinds += std::string{kind} + "=" + std::to_string(n) + " ";
        log().info(
            "Crafting recipes: {} views from {} game recipes; kinds: {}; duplicates={} broken={} no-display={}",
            out.size() - firstOut,
            it->second.size(),
            kinds,
            stats.duplicates,
            stats.broken,
            stats.skipped
        );
    }
};

} // namespace

BEI_REGISTER_RECIPE_PROVIDER(CraftingRecipeProvider);

} // namespace bei::game
