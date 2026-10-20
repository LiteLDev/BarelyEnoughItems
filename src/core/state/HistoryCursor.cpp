#include "core/state/HistoryCursor.h"

#include <algorithm>

namespace bei::core {

std::optional<std::string> HistoryCursor::older(std::vector<std::string> const& history, std::string const& current) {
    int const size = static_cast<int>(history.size());
    int       next = std::min(mIndex, size - 1) + 1; // the list may have shrunk while navigating
    auto      skip = [&](std::string const& entry) { return entry == current || (mIndex >= 0 && entry == mDraft); };
    while (next < size && skip(history[static_cast<std::size_t>(next)])) ++next;
    if (next >= size) return std::nullopt;
    if (mIndex < 0) mDraft = current;
    mIndex = next;
    return history[static_cast<std::size_t>(next)];
}

std::optional<std::string> HistoryCursor::newer(std::vector<std::string> const& history, std::string const& current) {
    if (mIndex < 0) return std::nullopt;
    int const size = static_cast<int>(history.size());
    int       next = std::min(mIndex, size) - 1;
    // Entries showing what is already in the box, or what Down would end on, would look like a
    // key press that does nothing.
    while (next >= 0 && (history[static_cast<std::size_t>(next)] == current
                         || history[static_cast<std::size_t>(next)] == mDraft)) {
        --next;
    }
    if (next < 0) {
        mIndex = -1;
        return mDraft;
    }
    mIndex = next;
    return history[static_cast<std::size_t>(next)];
}

} // namespace bei::core
