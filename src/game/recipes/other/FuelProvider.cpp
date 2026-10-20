// Fuel category: every item that burns in a furnace, with how many items it smelts.
#include "app/I18n.h"
#include "game/recipes/IRecipeProvider.h"
#include "game/recipes/other/Common.h"
#include "mod/Log.h"

#include <format>

namespace bei::game::other {

namespace {

/// Ticks a regular furnace needs per item.
constexpr int kTicksPerItem = 200;

std::string burnNote(int ticks) {
    // "{items}" items smelted, "{seconds}" burn time in seconds.
    std::string text    = app::tr("bei.fuel.burn_time", "Burns {seconds}s, smelts {items} items");
    auto        replace = [&](std::string_view token, std::string const& value) {
        for (auto pos = text.find(token); pos != std::string::npos; pos = text.find(token, pos + value.size())) {
            text.replace(pos, token.size(), value);
        }
    };
    double items = static_cast<double>(ticks) / kTicksPerItem;
    replace(
        "{items}",
        items == static_cast<int>(items) ? std::to_string(static_cast<int>(items)) : std::format("{:.1f}", items)
    );
    double seconds = ticks / 20.0;
    replace(
        "{seconds}",
        seconds == static_cast<int>(seconds) ? std::to_string(static_cast<int>(seconds))
                                             : std::format("{:.1f}", seconds)
    );
    return text;
}

class FuelProvider final : public IRecipeProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "fuel"; }

    void categories(std::vector<core::CategoryInfo>& out) const override {
        core::CategoryInfo info;
        info.id            = core::category::FUEL;
        info.titleKey      = "bei.category.fuel";
        info.fallbackTitle = "Fuel";
        info.layout        = core::LayoutKind::Generic;
        info.workstations  = {
            core::ItemKey{"minecraft:furnace",       core::ItemKey::ANY_AUX},
            core::ItemKey{"minecraft:blast_furnace", core::ItemKey::ANY_AUX},
            core::ItemKey{"minecraft:smoker",        core::ItemKey::ANY_AUX},
        };
        info.order = 60;
        out.push_back(std::move(info));
    }

    void collect(Level& level, Recipes&, std::vector<core::RecipeView>& out) const override {
        auto fuels = collectFuels(level);
        for (auto const& fuel : fuels) {
            core::RecipeView view;
            view.id       = "bei:fuel/" + fuel.key.toString();
            view.category = core::category::FUEL;
            view.inputs.push_back(single(fuel.key));
            view.note = burnNote(fuel.burnTicks);
            out.push_back(std::move(view));
        }
        log().info("Recipes [{}]: {}", core::category::FUEL, fuels.size());
    }
};

} // namespace

} // namespace bei::game::other

BEI_REGISTER_RECIPE_PROVIDER(bei::game::other::FuelProvider);
