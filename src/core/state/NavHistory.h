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
    ItemKey     item; ///< Empty when browsing a whole category.
    LookupMode  mode = LookupMode::Recipes;
    std::string category;        ///< Category to show; empty = first available.
    int         categoryTab = 0; ///< Tab index the user selected (first tab visible).
    int         page        = 0; ///< Recipe page within the category.

    /// Same item and mode (ignores the tab/page the user moved to). For an item focus an empty
    /// category ("any") matches every category, since the tab the user picked fills it in; for a
    /// category browse (empty item) the categories must be equal.
    [[nodiscard]] bool sameTarget(RecipeFocus const& o) const noexcept {
        if (item != o.item || mode != o.mode) return false;
        if (category == o.category) return true;
        return !item.empty() && (category.empty() || o.category.empty());
    }

    friend bool operator==(RecipeFocus const&, RecipeFocus const&) = default;
};

/// Back-navigation stack for the recipe screen (JEI's Backspace behaviour).
///
/// The top of the stack is what the recipe screen currently shows. `back()` drops it and
/// returns the entry below, which the screen should show next; when nothing is left the
/// screen should close.
class NavHistory {
public:
    static constexpr std::size_t MAX_DEPTH = 64;

    /// Push a new focus. Pushing the same focus as current is a no-op. Oldest entries are
    /// dropped past maxDepth() (MAX_DEPTH by default).
    void push(RecipeFocus focus);
    /// Like push(), but if the current entry has the same target (item/mode/category) it is
    /// replaced instead of stacking a duplicate (see RecipeFocus::sameTarget). A focus with an empty
    /// category leaves a current entry that already has a category untouched, so re-opening the
    /// shown item keeps the user's tab. Returns true if a new entry was pushed.
    bool pushOrReplace(RecipeFocus focus);
    /// Pop the current focus and return the new current one (nullopt if history became empty).
    /// Calling back() on an empty history is a no-op that returns nullopt.
    std::optional<RecipeFocus> back();
    /// Mutable access to the current entry (to persist page/tab changes), or nullptr.
    [[nodiscard]] RecipeFocus*       current() noexcept;
    [[nodiscard]] RecipeFocus const* current() const noexcept;
    /// The entry back() would return, without popping (nullptr if back() would empty the history).
    [[nodiscard]] RecipeFocus const* previous() const noexcept;
    /// True if back() would leave something to show.
    [[nodiscard]] bool        canGoBack() const noexcept { return mStack.size() > 1; }
    [[nodiscard]] bool        empty() const noexcept { return mStack.empty(); }
    void                      clear() noexcept { mStack.clear(); }
    [[nodiscard]] std::size_t depth() const noexcept { return mStack.size(); }
    /// Oldest first; the last element is current().
    [[nodiscard]] std::vector<RecipeFocus> const& entries() const noexcept { return mStack; }

    /// Update the current entry's tab (and category, if non-empty). The page resets to 0 when
    /// the tab or category changes. Negative tabs clamp to 0. Returns false if empty.
    bool setCurrentTab(int tab, std::string category = {});
    /// Update the current entry's page (negative clamps to 0). Returns false if empty.
    bool setCurrentPage(int page);

    /// Change the maximum depth (minimum 1); trims the oldest entries if needed.
    void                      setMaxDepth(std::size_t depth);
    [[nodiscard]] std::size_t maxDepth() const noexcept { return mMaxDepth; }

private:
    void trim();

    std::vector<RecipeFocus> mStack;
    std::size_t              mMaxDepth = MAX_DEPTH;
};

} // namespace bei::core
