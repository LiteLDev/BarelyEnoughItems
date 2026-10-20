// Minimal implementation: whitespace-separated ASCII-case-insensitive substring terms.
// Unit "Search engine" implements the full syntax, pinyin and caching.
#include "core/search/SearchEngine.h"

#include <algorithm>
#include <cctype>

namespace bei::core {

namespace {
std::string lower(std::string_view s) {
    std::string out{s};
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<std::string> terms(std::string_view query) {
    std::vector<std::string> out;
    std::string              cur;
    for (char c : query) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) out.push_back(lower(cur)), cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(lower(cur));
    return out;
}

bool entryMatches(ItemEntry const& e, std::vector<std::string> const& ts) {
    auto name = lower(e.displayName);
    auto en   = lower(e.englishName);
    auto id   = lower(e.key.id);
    return std::all_of(ts.begin(), ts.end(), [&](std::string const& t) {
        return name.find(t) != std::string::npos || en.find(t) != std::string::npos
            || id.find(t) != std::string::npos;
    });
}
} // namespace

void SearchEngine::setItems(std::vector<ItemEntry> const* items) { mItems = items; }

bool SearchEngine::matches(ItemEntry const& entry, std::string_view query) const {
    return entryMatches(entry, terms(query));
}

std::vector<std::size_t> SearchEngine::filter(std::string_view query) const {
    std::vector<std::size_t> out;
    if (!mItems) return out;
    auto ts = terms(query);
    for (std::size_t i = 0; i < mItems->size(); ++i) {
        if (ts.empty() || entryMatches((*mItems)[i], ts)) out.push_back(i);
    }
    return out;
}

} // namespace bei::core
