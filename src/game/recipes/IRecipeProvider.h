#pragma once

#include "core/model/RecipeView.h"

#include <memory>
#include <string_view>
#include <vector>

class Level;
class Recipes;

namespace bei::game {

/// Converts one family of game recipes into core::RecipeView.
///
/// Providers are stateless converters registered at static-init time with
/// BEI_REGISTER_RECIPE_PROVIDER. RecipeManager calls them on the client thread whenever the
/// recipe set changes (level join, CraftingDataPacket, resource reload).
class IRecipeProvider {
public:
    virtual ~IRecipeProvider() = default;

    /// Short name for logs, e.g. "crafting".
    [[nodiscard]] virtual std::string_view name() const = 0;

    /// Categories this provider produces (title, layout, workstations).
    virtual void categories(std::vector<core::CategoryInfo>& out) const = 0;

    /// Append all recipes. Must not keep pointers into game objects.
    virtual void collect(Level& level, Recipes& recipes, std::vector<core::RecipeView>& out) const = 0;
};

class RecipeProviderRegistry {
public:
    static RecipeProviderRegistry& get();

    void add(std::unique_ptr<IRecipeProvider> provider) { mProviders.push_back(std::move(provider)); }
    [[nodiscard]] std::vector<std::unique_ptr<IRecipeProvider>> const& providers() const noexcept { return mProviders; }

private:
    std::vector<std::unique_ptr<IRecipeProvider>> mProviders;
};

} // namespace bei::game

#define BEI_CONCAT_IMPL(a, b) a##b
#define BEI_CONCAT(a, b)      BEI_CONCAT_IMPL(a, b)

/// Register a provider type (default constructible) from its .cpp file.
#define BEI_REGISTER_RECIPE_PROVIDER(TYPE)                                                                             \
    [[maybe_unused]] static bool const BEI_CONCAT(beiRecipeProviderRegistered_, __LINE__) = [] {                       \
        ::bei::game::RecipeProviderRegistry::get().add(std::make_unique<TYPE>());                                      \
        return true;                                                                                                   \
    }()
