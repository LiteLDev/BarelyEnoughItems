// Brewing stand recipes.
//
// PotionBrewing's mix tables (Mix<T>) have no published layout, so instead of reading them we
// ask the game itself: for every potion container (potion, splash, lingering) and every potion
// id, try every registered brewing reagent with PotionBrewing::hasMix / PotionBrewing::mix.
// That covers potion mixes (awkward + nether wart...) and container mixes (+ gunpowder,
// + dragon's breath) with the exact aux values the game produces, so icons show the right potion.
#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/other/Common.h"
#include "mod/Log.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/alchemy/Potion.h"
#include "mc/world/item/alchemy/PotionBrewing.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/Level.h"

#include <set>

namespace bei::game::other {

namespace {

constexpr char const* kContainers[] = {"minecraft:potion", "minecraft:splash_potion", "minecraft:lingering_potion"};

class BrewingProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "brewing"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        core::CategoryInfo info;
        info.id            = core::category::BREWING;
        info.titleKey      = "bei.category.brewing";
        info.fallbackTitle = "Brewing";
        info.layout        = core::LayoutKind::Brewing;
        info.workstations  = {
            core::ItemKey{"minecraft:brewing_stand", core::ItemKey::ANY_AUX}
        };
        info.order = 40;
        out.push_back(std::move(info));
    }

    void collect(Level& level, Recipes&, std::vector<core::RecipeView>& out) const override {
        auto registry = level.getItemRegistry();

        // Potion ids that exist (aux values of potion items).
        std::vector<int> potionIds;
        auto const&      byId = Potion::mPotionsById();
        for (std::size_t i = 0; i < byId.size(); ++i) {
            if (byId[i]) potionIds.push_back(static_cast<int>(i));
        }

        // Reagents: every item the brewing stand accepts in its ingredient slot.
        struct Reagent {
            Item const*   item;
            int           aux;
            core::ItemKey key;
        };
        std::vector<Reagent> reagents;
        forEachRegisteredItem(level, [&](Item const& item) {
            ItemDescriptor desc{item, 0};
            if (!PotionBrewing::isPotionIngredient(desc) && !PotionBrewing::isIngredient(desc)) return;
            if (auto key = keyOf(item, 0); !key.empty()) reagents.push_back({&item, 0, std::move(key)});
        });

        core::IngredientView                              catalyst = single(core::ItemKey{"minecraft:blaze_powder", 0});
        std::set<std::pair<core::ItemKey, core::ItemKey>> seen; // (input, reagent)
        std::size_t                                       before = out.size();
        for (auto const* containerName : kContainers) {
            auto const* container = registry.lookupByName(HashedString{containerName}).get();
            if (!container) continue;
            for (int potionId : potionIds) {
                ItemInstance source;
                source.reinit(*container, 1, potionId);
                if (source.isNull()) continue;
                auto sourceKey = keyOf(source);
                if (sourceKey.empty()) continue;
                for (auto const& reagent : reagents) {
                    ItemDescriptor reagentDesc{*reagent.item, reagent.aux};
                    if (!PotionBrewing::hasMix(source, reagentDesc)) continue;
                    ItemInstance result    = PotionBrewing::mix(reagentDesc, source);
                    auto         resultKey = keyOf(result);
                    if (resultKey.empty() || resultKey == sourceKey) continue;
                    if (!seen.emplace(sourceKey, reagent.key).second) continue;
                    core::RecipeView view;
                    view.id       = "bei:brewing/" + sourceKey.toString() + "+" + reagent.key.toString();
                    view.category = core::category::BREWING;
                    view.inputs.push_back(single(sourceKey));
                    view.inputs.push_back(single(reagent.key));
                    view.outputs.push_back(single(resultKey));
                    view.catalysts.push_back(catalyst);
                    out.push_back(std::move(view));
                }
            }
        }
        log().info(
            "Recipes [{}]: {} ({} potion ids, {} reagents)",
            core::category::BREWING,
            out.size() - before,
            potionIds.size(),
            reagents.size()
        );
    }
};

} // namespace

} // namespace bei::game::other

BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::BrewingProvider);
