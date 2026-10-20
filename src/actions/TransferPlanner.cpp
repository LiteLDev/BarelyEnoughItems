#include "actions/TransferPlanner.h"

#include <algorithm>
#include <numeric>

namespace bei::actions {

Station stationFor(std::string_view category) noexcept {
    if (category == core::category::CRAFTING) return Station::Crafting;
    if (category == core::category::FURNACE) return Station::Furnace;
    if (category == core::category::BLAST_FURNACE) return Station::BlastFurnace;
    if (category == core::category::SMOKER) return Station::Smoker;
    if (category == core::category::SMITHING) return Station::SmithingTable;
    return Station::None;
}

std::string_view furnaceTagOf(Station station) noexcept {
    switch (station) {
    case Station::Furnace:
        return "furnace";
    case Station::BlastFurnace:
        return "blast_furnace";
    case Station::Smoker:
        return "smoker";
    default:
        return {};
    }
}

Station furnaceStationOf(std::string_view tag) noexcept {
    for (auto s : {Station::Furnace, Station::BlastFurnace, Station::Smoker}) {
        if (furnaceTagOf(s) == tag) return s;
    }
    return Station::None;
}

core::ItemKey
pickFuel(std::vector<core::ItemKey> const& fuels, std::vector<PoolEntry> const& pool, core::ItemKey const& ingredient) {
    for (auto const& fuel : fuels) {
        for (auto const& entry : pool) {
            if (entry.count <= 0 || !fuel.matches(entry.key)) continue;
            if (!ingredient.empty() && entry.key == ingredient) continue;
            return entry.key;
        }
    }
    return {};
}

GridMapping mapToGrid(core::RecipeView const& recipe, int gridSize) {
    GridMapping result;
    auto const  n = static_cast<int>(recipe.inputs.size());
    result.cellOfInput.assign(n, -1);
    if (gridSize <= 0) return result;

    int nonEmpty = 0;
    for (auto const& in : recipe.inputs) nonEmpty += in.empty() ? 0 : 1;
    if (nonEmpty == 0) return result;

    bool const shaped = !recipe.shapeless && recipe.width > 0 && recipe.height > 0 && recipe.width * recipe.height == n;
    if (!shaped) {
        if (nonEmpty > gridSize * gridSize) return result;
        int cell = 0;
        for (int i = 0; i < n; ++i) {
            if (!recipe.inputs[i].empty()) result.cellOfInput[i] = cell++;
        }
        result.fits = true;
        return result;
    }

    int minR = recipe.height, maxR = -1, minC = recipe.width, maxC = -1;
    for (int i = 0; i < n; ++i) {
        if (recipe.inputs[i].empty()) continue;
        int r = i / recipe.width, c = i % recipe.width;
        minR = std::min(minR, r), maxR = std::max(maxR, r);
        minC = std::min(minC, c), maxC = std::max(maxC, c);
    }
    if (maxR - minR + 1 > gridSize || maxC - minC + 1 > gridSize) return result;
    for (int i = 0; i < n; ++i) {
        if (recipe.inputs[i].empty()) continue;
        int r = i / recipe.width - minR, c = i % recipe.width - minC;
        result.cellOfInput[i] = r * gridSize + c;
    }
    result.fits = true;
    return result;
}

void addToPool(std::vector<PoolEntry>& pool, core::ItemKey const& key, int count, int maxStack) {
    if (key.empty() || count <= 0) return;
    for (auto& e : pool) {
        if (e.key == key) {
            e.count += count;
            return;
        }
    }
    pool.push_back({key, count, std::max(1, maxStack)});
}

Assignment assign(core::RecipeView const& recipe, std::vector<PoolEntry> const& pool, int sets) {
    Assignment result;
    auto const n = recipe.inputs.size();
    result.chosen.resize(n);

    std::vector<int> remaining(pool.size());
    for (std::size_t p = 0; p < pool.size(); ++p) remaining[p] = pool[p].count;

    // Candidate pool entries per input.
    std::vector<std::vector<std::size_t>> candidates(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t p = 0; p < pool.size(); ++p) {
            auto const& alts = recipe.inputs[i].alternatives;
            if (std::any_of(alts.begin(), alts.end(), [&](auto const& a) { return a.matches(pool[p].key); })) {
                candidates[i].push_back(p);
            }
        }
    }

    // Most constrained inputs first, so flexible ones (e.g. "any planks") don't steal specific items.
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return candidates[a].size() < candidates[b].size();
    });

    for (auto i : order) {
        auto const& in = recipe.inputs[i];
        if (in.empty()) continue;
        int const need = std::max(1, in.count) * std::max(1, sets);
        int       best = -1;
        for (auto p : candidates[i]) {
            if (remaining[p] < need || pool[p].maxStack < need) continue;
            if (best < 0 || remaining[p] > remaining[best]) best = static_cast<int>(p);
        }
        if (best < 0) {
            result.missing.push_back(static_cast<int>(i));
            continue;
        }
        remaining[best]  -= need;
        result.chosen[i]  = pool[best].key;
    }
    std::sort(result.missing.begin(), result.missing.end());
    return result;
}

int maxSets(core::RecipeView const& recipe, std::vector<PoolEntry> const& pool, int cap) {
    // The greedy assign() is not guaranteed to be monotonic in `sets`, so scan down from the
    // largest candidate (at most `cap` cheap calls).
    for (int sets = std::max(1, cap); sets >= 1; --sets) {
        if (assign(recipe, pool, sets).complete()) return sets;
    }
    return 0;
}

} // namespace bei::actions
