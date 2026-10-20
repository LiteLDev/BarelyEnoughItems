#include "core/recipe/RecipeIndex.h"

#include <algorithm>
#include <set>
#include <utility>

namespace bei::core {

namespace {

void appendUnique(std::vector<std::uint32_t>& list, std::uint32_t pos) {
    if (list.empty() || list.back() != pos) list.push_back(pos);
}

/// lower_bound over a vector of (aux, list) pairs sorted by aux.
template <class Exact>
auto lowerBoundAux(Exact& exact, std::int16_t aux) {
    return std::lower_bound(exact.begin(), exact.end(), aux, [](auto const& e, std::int16_t a) { return e.first < a; });
}

/// The posting list for exactly `aux`, or nullptr.
template <class Exact>
auto findAux(Exact const& exact, std::int16_t aux) -> decltype(&exact.front().second) {
    auto it = lowerBoundAux(exact, aux);
    return it != exact.end() && it->first == aux ? &it->second : nullptr;
}

} // namespace

void RecipeIndex::build(std::vector<CategoryInfo> categories, std::vector<RecipeView> recipes) {
    clear();

    std::stable_sort(categories.begin(), categories.end(), [](auto const& a, auto const& b) {
        return a.order != b.order ? a.order < b.order : a.id < b.id;
    });
    // Duplicate category ids: the first one (after sorting) wins, but workstations of all are kept.
    for (auto& c : categories) {
        auto [it, inserted] = mGroupRank.emplace(c.id, static_cast<std::uint32_t>(mCategories.size()));
        if (inserted) {
            mGroupCategory.push_back(c.id);
            mCategories.push_back(std::move(c));
            continue;
        }
        auto& stations = mCategories[it->second].workstations;
        for (auto& ws : c.workstations) {
            if (std::find(stations.begin(), stations.end(), ws) == stations.end()) stations.push_back(std::move(ws));
        }
    }

    {
        std::set<std::pair<std::string, std::string>> seen;
        mRecipes.reserve(recipes.size());
        for (auto& r : recipes) {
            if (!r.id.empty() && !seen.emplace(r.category, r.id).second) continue;
            mRecipes.push_back(std::move(r));
        }
    }

    // Group ranks: registered categories in display order (rank == mCategories index), then
    // categories that only appear on recipes, by id.
    {
        std::set<std::string> unknown;
        for (auto const& r : mRecipes) {
            if (!mGroupRank.contains(r.category)) unknown.insert(r.category);
        }
        for (auto const& id : unknown) {
            mGroupRank.emplace(id, static_cast<std::uint32_t>(mGroupCategory.size()));
            mGroupCategory.push_back(id);
        }
    }

    // Display order: counting sort by group rank, stable in insertion order.
    std::vector<std::uint32_t> recipeGroup(mRecipes.size());
    std::vector<Pos>           counts(mGroupCategory.size() + 1, 0);
    for (std::size_t i = 0; i < mRecipes.size(); ++i) {
        recipeGroup[i] = mGroupRank.at(mRecipes[i].category);
        ++counts[recipeGroup[i] + 1];
    }
    for (std::size_t g = 1; g < counts.size(); ++g) counts[g] += counts[g - 1];
    mGroupRange.resize(mGroupCategory.size());
    for (std::size_t g = 0; g < mGroupCategory.size(); ++g) mGroupRange[g] = {counts[g], counts[g + 1]};
    mOrder.resize(mRecipes.size());
    mPosGroup.resize(mRecipes.size());
    for (std::size_t i = 0; i < mRecipes.size(); ++i) {
        Pos pos        = counts[recipeGroup[i]]++;
        mOrder[pos]    = static_cast<std::uint32_t>(i);
        mPosGroup[pos] = recipeGroup[i];
    }

    // Posting lists, filled in ascending position order so every list stays sorted and duplicate-free.
    auto index = [](std::unordered_map<std::string, Postings>& map, std::vector<IngredientView> const& slots, Pos pos) {
        for (auto const& slot : slots) {
            for (auto const& alt : slot.alternatives) {
                if (alt.id.empty()) continue;
                auto& p = map[alt.id];
                appendUnique(p.any, pos);
                if (alt.isWildcard()) {
                    appendUnique(p.wildcard, pos);
                    continue;
                }
                auto it = lowerBoundAux(p.exact, alt.aux);
                if (it == p.exact.end() || it->first != alt.aux) it = p.exact.insert(it, {alt.aux, {}});
                appendUnique(it->second, pos);
            }
        }
    };
    for (Pos pos = 0; pos < mOrder.size(); ++pos) {
        auto const& r = mRecipes[mOrder[pos]];
        index(mOutputs, r.outputs, pos);
        index(mInputs, r.inputs, pos);
    }

    for (std::uint32_t group = 0; group < mCategories.size(); ++group) {
        for (auto const& ws : mCategories[group].workstations) {
            if (!ws.empty()) mStations[ws.id].push_back(Station{ws, group});
        }
    }
}

void RecipeIndex::clear() {
    mCategories.clear();
    mRecipes.clear();
    mOrder.clear();
    mPosGroup.clear();
    mGroupCategory.clear();
    mGroupRange.clear();
    mGroupRank.clear();
    mOutputs.clear();
    mInputs.clear();
    mStations.clear();
}

CategoryInfo const* RecipeIndex::findCategory(std::string const& id) const {
    auto it = mGroupRank.find(id);
    return it == mGroupRank.end() || it->second >= mCategories.size() ? nullptr : &mCategories[it->second];
}

std::vector<std::size_t> RecipeIndex::recipesInCategory(std::string const& category) const {
    std::vector<std::size_t> out;
    auto                     it = mGroupRank.find(category);
    if (it == mGroupRank.end()) return out;
    auto [begin, end] = mGroupRange[it->second];
    out.reserve(end - begin);
    for (Pos pos = begin; pos < end; ++pos) out.push_back(mOrder[pos]);
    return out;
}

std::size_t RecipeIndex::categorySize(std::string const& category) const {
    auto it = mGroupRank.find(category);
    if (it == mGroupRank.end()) return 0;
    auto [begin, end] = mGroupRange[it->second];
    return end - begin;
}

template <class Fn>
void RecipeIndex::forEachMatch(ItemKey const& item, LookupMode mode, Fn&& fn) const {
    auto const& map = mode == LookupMode::Recipes ? mOutputs : mInputs;
    auto        it  = map.find(item.id);
    if (it == map.end()) return;
    auto const& p = it->second;
    if (item.isWildcard()) {
        for (auto pos : p.any) fn(pos);
        return;
    }
    // Merge the exact-aux list with the wildcard list (both sorted, possibly overlapping).
    static std::vector<Pos> const none;
    auto const*                   exactList = findAux(p.exact, item.aux);
    auto const&                   a         = exactList ? *exactList : none;
    auto const&                   b         = p.wildcard;
    std::size_t                   i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        if (j == b.size() || (i < a.size() && a[i] < b[j])) {
            fn(a[i++]);
        } else if (i == a.size() || b[j] < a[i]) {
            fn(b[j++]);
        } else {
            fn(a[i]);
            ++i;
            ++j;
        }
    }
}

std::vector<std::uint32_t> RecipeIndex::stationGroups(ItemKey const& item) const {
    std::vector<std::uint32_t> out;
    auto                       it = mStations.find(item.id);
    if (it == mStations.end()) return out;
    for (auto const& s : it->second) {
        if (!s.key.matches(item)) continue;
        if (mGroupRange[s.group].first == mGroupRange[s.group].second) continue; // no recipes
        out.push_back(s.group);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::vector<CategoryGroup> RecipeIndex::lookup(ItemKey const& item, LookupMode mode) const {
    std::vector<CategoryGroup> groups;
    if (item.empty()) return groups;

    std::vector<std::uint32_t> stations;
    if (mode == LookupMode::Uses) stations = stationGroups(item);
    for (auto g : stations) {
        auto& out   = groups.emplace_back(CategoryGroup{mGroupCategory[g], {}});
        auto [b, e] = mGroupRange[g];
        out.recipes.reserve(e - b);
        for (Pos pos = b; pos < e; ++pos) out.recipes.push_back(mOrder[pos]);
    }

    std::uint32_t current = UINT32_MAX; // group of groups.back() among direct matches
    forEachMatch(item, mode, [&](Pos pos) {
        auto g = mPosGroup[pos];
        if (std::binary_search(stations.begin(), stations.end(), g)) return; // already listed in full
        if (g != current) {
            current = g;
            groups.emplace_back(CategoryGroup{mGroupCategory[g], {}});
        }
        groups.back().recipes.push_back(mOrder[pos]);
    });
    return groups;
}

std::size_t RecipeIndex::lookupCount(ItemKey const& item, LookupMode mode) const {
    if (item.empty()) return 0;
    std::vector<std::uint32_t> stations;
    if (mode == LookupMode::Uses) stations = stationGroups(item);
    std::size_t total = 0;
    for (auto g : stations) total += mGroupRange[g].second - mGroupRange[g].first;
    forEachMatch(item, mode, [&](Pos pos) {
        if (!std::binary_search(stations.begin(), stations.end(), mPosGroup[pos])) ++total;
    });
    return total;
}

bool RecipeIndex::has(ItemKey const& item, LookupMode mode) const {
    if (item.empty()) return false;
    auto const& map = mode == LookupMode::Recipes ? mOutputs : mInputs;
    if (auto it = map.find(item.id); it != map.end()) {
        auto const& p = it->second;
        if (item.isWildcard()) {
            if (!p.any.empty()) return true;
        } else if (!p.wildcard.empty() || findAux(p.exact, item.aux)) {
            return true;
        }
    }
    if (mode == LookupMode::Uses) {
        if (auto it = mStations.find(item.id); it != mStations.end()) {
            for (auto const& s : it->second) {
                if (s.key.matches(item) && mGroupRange[s.group].first != mGroupRange[s.group].second) return true;
            }
        }
    }
    return false;
}

} // namespace bei::core
