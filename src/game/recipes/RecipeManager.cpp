#include "game/recipes/RecipeManager.h"

#include "game/Game.h"
#include "game/recipes/IRecipeProvider.h"
#include "mod/Log.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"

#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/Recipes.h"

#include <chrono>
#include <map>

namespace bei::game {

namespace {
ll::event::ListenerPtr gJoin;
ll::event::ListenerPtr gExit;
ll::event::ListenerPtr gTick;
} // namespace

RecipeProviderRegistry& RecipeProviderRegistry::get() {
    static RecipeProviderRegistry instance;
    return instance;
}

RecipeManager& RecipeManager::get() {
    static RecipeManager instance;
    return instance;
}

void RecipeManager::install() {
    auto& bus = ll::event::EventBus::getInstance();
    gJoin     = bus.emplaceListener<ll::event::ClientJoinLevelEvent>([this](auto&) {
        mDirty = true;
        if (auto* level = currentLevel()) {
            mListenerLife = std::make_shared<bool>(true);
            level->getRecipes().addRecipeListener(mListenerLife, [this] { mDirty = true; });
        }
    });
    gExit     = bus.emplaceListener<ll::event::ClientExitLevelEvent>([this](auto&) { clear(); });
    gTick     = bus.emplaceListener<ll::event::ClientLevelTickEvent>([this](auto&) {
        if (mDirty) rebuild();
    });
}

void RecipeManager::uninstall() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* l : {&gJoin, &gExit, &gTick}) {
        if (*l) bus.removeListener(*l);
        l->reset();
    }
    clear();
}

void RecipeManager::clear() {
    mListenerLife.reset();
    mIndex.clear();
    mDirty = true;
    ++mGeneration;
}

core::RecipeIndex const& RecipeManager::index() {
    if (mDirty) rebuild();
    return mIndex;
}

void RecipeManager::rebuild() {
    auto* level = currentLevel();
    if (!level) return;
    auto  start   = std::chrono::steady_clock::now();
    auto& recipes = level->getRecipes();

    std::vector<core::CategoryInfo> categories;
    std::vector<core::RecipeView>   views;
    for (auto const& provider : RecipeProviderRegistry::get().providers()) {
        try {
            provider->categories(categories);
            auto before = views.size();
            provider->collect(*level, recipes, views);
            log().debug("Recipe provider '{}': {} recipes", provider->name(), views.size() - before);
        } catch (std::exception const& e) {
            log().error("Recipe provider '{}' failed: {}", provider->name(), e.what());
        }
    }
    mIndex.build(std::move(categories), std::move(views));
    mDirty = false;
    ++mGeneration;

    // Diagnostic: raw recipe counts per game tag, to spot tags no provider handles yet.
    std::map<std::string, std::size_t> perTag;
    for (auto const& [tag, byId] : recipes.mRecipes.get()) perTag[tag.getString()] += byId.size();
    std::string summary;
    for (auto const& [tag, n] : perTag) summary += tag + "=" + std::to_string(n) + " ";
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    log().info("Recipe index rebuilt: {} recipes in {} categories ({} ms). Game tags: {}",
        mIndex.size(), mIndex.categories().size(), ms, summary);
}

} // namespace bei::game
