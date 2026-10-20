#include "core/state/NavHistory.h"

#include <utility>

namespace bei::core {

void NavHistory::push(RecipeFocus focus) {
    if (!mStack.empty() && mStack.back() == focus) return;
    mStack.push_back(std::move(focus));
    if (mStack.size() > MAX_DEPTH) mStack.erase(mStack.begin());
}

std::optional<RecipeFocus> NavHistory::back() {
    if (mStack.empty()) return std::nullopt;
    mStack.pop_back();
    if (mStack.empty()) return std::nullopt;
    return mStack.back();
}

RecipeFocus* NavHistory::current() noexcept { return mStack.empty() ? nullptr : &mStack.back(); }

} // namespace bei::core
