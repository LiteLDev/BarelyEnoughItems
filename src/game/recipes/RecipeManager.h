#pragma once

#include "core/recipe/RecipeIndex.h"

#include <cstdint>
#include <memory>

namespace bei::game {

/// Owns the RecipeIndex for the current level and keeps it in sync with the game.
///
/// The index is rebuilt lazily on the client thread (on the next tick or the next index()
/// call) after: level join, the game notifying recipe listeners (CraftingDataPacket), or
/// markDirty().
class RecipeManager {
public:
    static RecipeManager& get();

    void install();
    void uninstall();

    /// Current index; rebuilds first if dirty and a level is available.
    [[nodiscard]] core::RecipeIndex const& index();
    /// Incremented on every rebuild; consumers cache derived data by generation.
    [[nodiscard]] std::uint64_t            generation() const noexcept { return mGeneration; }

    void markDirty() noexcept { mDirty = true; }
    void rebuild();
    void clear();

private:
    core::RecipeIndex     mIndex;
    std::uint64_t         mGeneration = 0;
    bool                  mDirty      = true;
    std::shared_ptr<bool> mListenerLife; // lifetime token for Recipes::addRecipeListener
};

} // namespace bei::game
