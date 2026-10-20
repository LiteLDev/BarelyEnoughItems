// Furnace, blast furnace, smoker, campfire and soul campfire recipes.
//
// The game stores these as one-ingredient recipes under the tag of each station. Results are
// also cached in Recipes::mFurnaceResults (tag hash -> item id/aux -> result); cache entries
// without a matching tagged recipe are added too, so nothing is lost.
#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/other/Common.h"
#include "mod/Log.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/RecipeIngredient.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"

#include <set>
#include <utility>

namespace bei::game::other {

namespace {

struct Station {
    char const* tag;
    char const* category;
    char const* titleKey;
    char const* fallbackTitle;
    char const* workstation;
    int         order;
    bool        usesFuel;
};

constexpr Station kStations[] = {
    {"furnace",       core::category::FURNACE, "bei.category.furnace", "Smelting",              "minecraft:furnace", 10, true},
    {"blast_furnace",
     core::category::BLAST_FURNACE,
     "bei.category.blast_furnace",                                     "Blasting",
     "minecraft:blast_furnace",                                                                                      11,
     true                                                                                                                    },
    {"smoker",        core::category::SMOKER,  "bei.category.smoker",  "Smoking",               "minecraft:smoker",  12, true},
    {"campfire",
     core::category::CAMPFIRE,
     "bei.category.campfire",                                          "Campfire Cooking",
     "minecraft:campfire",                                                                                           13,
     false                                                                                                                   },
    {"soul_campfire",
     core::category::SOUL_CAMPFIRE,
     "bei.category.soul_campfire",                                     "Soul Campfire Cooking",
     "minecraft:soul_campfire",                                                                                      14,
     false                                                                                                                   },
};

class SmeltingProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "smelting"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        for (auto const& s : kStations) {
            core::CategoryInfo info;
            info.id            = s.category;
            info.titleKey      = s.titleKey;
            info.fallbackTitle = s.fallbackTitle;
            info.layout        = core::LayoutKind::Smelting;
            info.workstations  = {
                core::ItemKey{s.workstation, core::ItemKey::ANY_AUX}
            };
            info.order = s.order;
            out.push_back(std::move(info));
        }
    }

    void collect(Level& level, Recipes& recipes, std::vector<core::RecipeView>& out) const override {
        core::IngredientView fuel;
        fuel.alternatives = fuelKeys(collectFuels(level));

        for (auto const& s : kStations) {
            std::size_t before  = out.size();
            std::size_t ordinal = 0;
            // (input, output) pairs already produced, to dedupe mFurnaceResults below.
            std::set<std::pair<core::ItemKey, core::ItemKey>> seen;
            forEachRecipeWithTag(recipes, s.tag, [&](std::string const& id, Recipe const& recipe) {
                ++ordinal;
                auto const& ingredients = recipe.mMyIngredients.get();
                if (ingredients.empty()) return;
                core::RecipeView view;
                view.id       = id.empty() ? recipeIdOf(recipe, s.tag, ordinal) : id;
                view.category = s.category;
                view.inputs.push_back(toView(ingredients.front()));
                view.outputs = resultsOf(level, recipe);
                if (view.inputs.front().empty() || view.outputs.empty()) return;
                view.inputs.front().count = 1; // a furnace always consumes one item
                if (s.usesFuel && !fuel.empty()) view.catalysts.push_back(fuel);
                view.netId = netIdOf(recipe);
                for (auto const& in : view.inputs.front().alternatives) {
                    seen.emplace(in, view.outputs.front().alternatives.front());
                }
                out.push_back(std::move(view));
            });
            addCachedResults(level, recipes, s, s.usesFuel ? fuel : core::IngredientView{}, seen, out);
            log().info("Recipes [{}]: {}", s.category, out.size() - before);
        }
    }

private:
    /// Recipes::mFurnaceResults: (int)hash(tag) -> (itemId << 16 | aux) -> result. It mirrors the
    /// tagged recipes; only pairs not produced from them already are added.
    static void addCachedResults(
        Level&                                             level,
        Recipes&                                           recipes,
        Station const&                                     station,
        core::IngredientView const&                        fuel,
        std::set<std::pair<core::ItemKey, core::ItemKey>>& seen,
        std::vector<core::RecipeView>&                     out
    ) {
        auto const& cache = recipes.mFurnaceResults.get();
        auto        it = cache.find(static_cast<int>(static_cast<std::uint32_t>(HashedString{station.tag}.getHash())));
        if (it == cache.end()) return;
        auto registry = level.getItemRegistry();
        int  added    = 0;
        for (auto const& [idAux, result] : it->second) {
            auto        weak = registry.getItem(static_cast<short>(static_cast<std::uint32_t>(idAux) >> 16));
            auto const* item = weak.get();
            if (!item) continue;
            auto in  = keyOf(*item, static_cast<std::int16_t>(idAux & 0xFFFF));
            auto res = keyOf(result);
            if (in.empty() || res.empty()) continue;
            bool known = false;
            for (auto const& [seenIn, seenOut] : seen) {
                if (seenOut == res && seenIn.matches(in)) {
                    known = true;
                    break;
                }
            }
            if (known || !seen.emplace(in, res).second) continue;
            core::RecipeView view;
            view.id       = std::string{"bei:"} + station.tag + "_cache/" + in.toString();
            view.category = station.category;
            view.inputs.push_back(single(in));
            view.outputs.push_back(single(res, result.mCount));
            if (!fuel.empty()) view.catalysts.push_back(fuel);
            out.push_back(std::move(view));
            ++added;
        }
        if (added > 0) log().info("Recipes [{}]: {} only in the furnace result cache", station.category, added);
    }
};

} // namespace

} // namespace bei::game::other

BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::SmeltingProvider);
