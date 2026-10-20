// Straightforward linear-scan implementation. Correct but O(recipes) per lookup;
// Unit "Recipe index" replaces this with real inverted indices.
#include "core/recipe/RecipeIndex.h"

#include <algorithm>
#include <set>
#include <utility>

namespace bei::core {

namespace {
bool anyMatches(std::vector<IngredientView> const& slots, ItemKey const& item) {
    for (auto const& slot : slots) {
        for (auto const& alt : slot.alternatives) {
            if (alt.matches(item)) return true;
        }
    }
    return false;
}
} // namespace

void RecipeIndex::build(std::vector<CategoryInfo> categories, std::vector<RecipeView> recipes) {
    std::stable_sort(categories.begin(), categories.end(), [](auto const& a, auto const& b) {
        return a.order != b.order ? a.order < b.order : a.id < b.id;
    });
    mCategories = std::move(categories);
    mRecipes.clear();
    std::set<std::pair<std::string, std::string>> seen;
    for (auto& r : recipes) {
        if (!r.id.empty() && !seen.emplace(r.category, r.id).second) continue;
        mRecipes.push_back(std::move(r));
    }
}

void RecipeIndex::clear() {
    mCategories.clear();
    mRecipes.clear();
}

CategoryInfo const* RecipeIndex::findCategory(std::string const& id) const {
    for (auto const& c : mCategories) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

std::vector<std::size_t> RecipeIndex::recipesInCategory(std::string const& category) const {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < mRecipes.size(); ++i) {
        if (mRecipes[i].category == category) out.push_back(i);
    }
    return out;
}

std::vector<CategoryGroup> RecipeIndex::lookup(ItemKey const& item, LookupMode mode) const {
    std::vector<CategoryGroup> groups;
    auto groupFor = [&](std::string const& cat) -> CategoryGroup& {
        for (auto& g : groups) {
            if (g.category == cat) return g;
        }
        return groups.emplace_back(CategoryGroup{cat, {}});
    };
    if (mode == LookupMode::Uses) {
        for (auto const& c : mCategories) {
            for (auto const& ws : c.workstations) {
                if (ws.matches(item)) {
                    groupFor(c.id).recipes = recipesInCategory(c.id);
                    break;
                }
            }
        }
    }
    for (std::size_t i = 0; i < mRecipes.size(); ++i) {
        auto const& r   = mRecipes[i];
        bool        hit = mode == LookupMode::Recipes ? anyMatches(r.outputs, item) : anyMatches(r.inputs, item);
        if (!hit) continue;
        auto& g = groupFor(r.category);
        if (std::find(g.recipes.begin(), g.recipes.end(), i) == g.recipes.end()) g.recipes.push_back(i);
    }
    std::erase_if(groups, [](auto const& g) { return g.recipes.empty(); });
    auto rank = [&](std::string const& id) -> std::size_t {
        for (std::size_t i = 0; i < mCategories.size(); ++i) {
            if (mCategories[i].id == id) return i;
        }
        return mCategories.size();
    };
    std::stable_sort(groups.begin(), groups.end(), [&](auto const& a, auto const& b) {
        auto ra = rank(a.category), rb = rank(b.category);
        return ra != rb ? ra < rb : a.category < b.category;
    });
    return groups;
}

bool RecipeIndex::has(ItemKey const& item, LookupMode mode) const { return !lookup(item, mode).empty(); }

} // namespace bei::core
