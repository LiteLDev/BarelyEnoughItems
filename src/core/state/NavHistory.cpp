#include "core/state/NavHistory.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace bei::core {

void NavHistory::trim() {
    if (mStack.size() > mMaxDepth) {
        mStack.erase(mStack.begin(), mStack.begin() + static_cast<std::ptrdiff_t>(mStack.size() - mMaxDepth));
    }
}

void NavHistory::push(RecipeFocus focus) {
    if (!mStack.empty() && mStack.back() == focus) return;
    mStack.push_back(std::move(focus));
    trim();
}

bool NavHistory::pushOrReplace(RecipeFocus focus) {
    if (!mStack.empty() && mStack.back().sameTarget(focus)) {
        // An "any category" request for the item already shown keeps the user's current tab.
        if (!(focus.category.empty() && !mStack.back().category.empty())) mStack.back() = std::move(focus);
        return false;
    }
    mStack.push_back(std::move(focus));
    trim();
    return true;
}

std::optional<RecipeFocus> NavHistory::back() {
    if (mStack.empty()) return std::nullopt;
    mStack.pop_back();
    if (mStack.empty()) return std::nullopt;
    return mStack.back();
}

RecipeFocus* NavHistory::current() noexcept { return mStack.empty() ? nullptr : &mStack.back(); }

RecipeFocus const* NavHistory::current() const noexcept { return mStack.empty() ? nullptr : &mStack.back(); }

RecipeFocus const* NavHistory::previous() const noexcept {
    return mStack.size() < 2 ? nullptr : &mStack[mStack.size() - 2];
}

bool NavHistory::setCurrentTab(int tab, std::string category) {
    auto* cur = current();
    if (!cur) return false;
    tab = std::max(tab, 0);
    if (cur->categoryTab != tab || (!category.empty() && cur->category != category)) cur->page = 0;
    cur->categoryTab = tab;
    if (!category.empty()) cur->category = std::move(category);
    return true;
}

bool NavHistory::setCurrentPage(int page) {
    auto* cur = current();
    if (!cur) return false;
    cur->page = std::max(page, 0);
    return true;
}

void NavHistory::setMaxDepth(std::size_t depth) {
    mMaxDepth = std::max<std::size_t>(depth, 1);
    trim();
}

} // namespace bei::core
