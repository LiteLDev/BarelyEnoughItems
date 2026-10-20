#pragma once

#include "core/model/ItemKey.h"
#include "core/recipe/RecipeIndex.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bei::core {

/// What the recipe screen is showing.
struct RecipeFocus {
    ItemKey     item;              ///< Empty when browsing a whole category.
    LookupMode  mode = LookupMode::Recipes;
    std::string category;          ///< Category to show; empty = first available.
    int         categoryTab   = 0; ///< Tab index the user selected (first tab visible).
    int         page          = 0; ///< Recipe page within the category.

    friend bool operator==(RecipeFocus const&, RecipeFocus const&) = default;
};

/// Back-navigation stack for the recipe screen (JEI's Backspace behaviour).
class NavHistory {
public:
    static constexpr std::size_t MAX_DEPTH = 64;

    /// Push a new focus. Pushing the same focus as current is a no-op. Oldest entries are
    /// dropped past MAX_DEPTH.
    void push(RecipeFocus focus);
    /// Pop the current focus and return the new current one (nullopt if history became empty).
    std::optional<RecipeFocus> back();
    /// Mutable access to the current entry (to persist page/tab changes), or nullptr.
    [[nodiscard]] RecipeFocus* current() noexcept;
    void                       clear() noexcept { mStack.clear(); }
    [[nodiscard]] std::size_t  depth() const noexcept { return mStack.size(); }

private:
    std::vector<RecipeFocus> mStack;
};

} // namespace bei::core
