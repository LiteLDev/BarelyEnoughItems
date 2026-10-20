// Composting category (JEI's "Composting"): every item the composter accepts, with the chance
// that it raises the compost level by one layer. The chance comes from the game itself
// (ComposterBlock::getFillChance, which covers the built-in table and data-driven
// "compostable" item components).
#include "app/I18n.h"
#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/other/Common.h"
#include "mod/Log.h"

#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/ComposterBlock.h"

#include <algorithm>
#include <chrono>
#include <string>

namespace bei::game::other {

namespace {

class CompostingProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "composting"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        core::CategoryInfo info;
        info.id            = core::category::COMPOSTING;
        info.titleKey      = "bei.category.composting";
        info.fallbackTitle = "Composting";
        info.layout        = core::LayoutKind::Generic;
        info.workstations  = {
            core::ItemKey{"minecraft:composter", core::ItemKey::ANY_AUX}
        };
        info.order = 70;
        out.push_back(std::move(info));
    }

    void collect(Level& level, Recipes&, std::vector<core::RecipeView>& out) const override {
        auto const start = std::chrono::steady_clock::now();
        struct Compostable {
            core::ItemKey key;
            int           chance = 0;
        };
        std::vector<Compostable> found;
        forEachRegisteredItem(level, [&](Item const& item) {
            auto key = keyOf(item, 0);
            if (key.empty()) return;
            ItemInstance stack;
            stack.reinit(item, 1, 0);
            if (stack.isNull()) return;
            int const chance = ComposterBlock::getFillChance(stack);
            if (chance > 0) found.push_back({std::move(key), std::min(chance, 100)});
        });
        // Highest chance first (JEI orders the same way), then by id.
        std::sort(found.begin(), found.end(), [](Compostable const& a, Compostable const& b) {
            return a.chance != b.chance ? a.chance > b.chance : a.key < b.key;
        });
        // Translated here like the fuel notes ("{chance}" = percent).
        std::string const format = app::tr("bei.composting.chance", "Compost level +1: {chance}% chance");
        for (auto const& c : found) {
            core::RecipeView view;
            view.id       = "bei:composting/" + c.key.toString();
            view.category = core::category::COMPOSTING;
            view.inputs.push_back(single(c.key));
            std::string const value = std::to_string(c.chance);
            std::string       note  = format;
            for (auto pos = note.find("{chance}"); pos != std::string::npos; pos = note.find("{chance}", pos + value.size())) {
                note.replace(pos, 8, value);
            }
            view.note = std::move(note);
            out.push_back(std::move(view));
        }
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        log().info("Recipes [{}]: {} ({} ms)", core::category::COMPOSTING, found.size(), ms.count());
    }
};

} // namespace

} // namespace bei::game::other

BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::CompostingProvider);
