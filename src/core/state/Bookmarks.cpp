#include "core/state/Bookmarks.h"

#include "core/state/AtomicWrite.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <unordered_set>

namespace bei::core {

namespace {
constexpr int FILE_VERSION = 1;
}

bool Bookmarks::contains(ItemKey const& item) const {
    return std::find(mItems.begin(), mItems.end(), item) != mItems.end();
}

std::optional<std::size_t> Bookmarks::indexOf(ItemKey const& item) const {
    auto it = std::find(mItems.begin(), mItems.end(), item);
    if (it == mItems.end()) return std::nullopt;
    return static_cast<std::size_t>(it - mItems.begin());
}

void Bookmarks::add(ItemKey const& item) { insert(0, item); }

bool Bookmarks::insert(std::size_t index, ItemKey const& item) {
    if (item.empty() || full() || contains(item)) return false;
    index = std::min(index, mItems.size());
    mItems.insert(mItems.begin() + static_cast<std::ptrdiff_t>(index), item);
    return true;
}

void Bookmarks::remove(ItemKey const& item) { std::erase(mItems, item); }

bool Bookmarks::toggle(ItemKey const& item) {
    if (contains(item)) {
        remove(item);
        return false;
    }
    add(item);
    return contains(item);
}

bool Bookmarks::move(std::size_t from, std::size_t to) {
    if (from >= mItems.size()) return false;
    to = std::min(to, mItems.size() - 1);
    if (from == to) return true;
    auto first = mItems.begin();
    if (from < to) {
        std::rotate(
            first + static_cast<std::ptrdiff_t>(from),
            first + static_cast<std::ptrdiff_t>(from) + 1,
            first + static_cast<std::ptrdiff_t>(to) + 1
        );
    } else {
        std::rotate(
            first + static_cast<std::ptrdiff_t>(to),
            first + static_cast<std::ptrdiff_t>(from),
            first + static_cast<std::ptrdiff_t>(from) + 1
        );
    }
    return true;
}

bool Bookmarks::moveTo(ItemKey const& item, std::size_t index) {
    auto from = indexOf(item);
    return from && move(*from, index);
}

void Bookmarks::setMaxCount(std::size_t count) {
    mMaxCount = std::max<std::size_t>(count, 1);
    if (mItems.size() > mMaxCount) mItems.resize(mMaxCount);
}

bool Bookmarks::fromJsonText(std::string_view text) {
    mItems.clear();
    auto j = nlohmann::json::parse(text, nullptr, false);
    if (j.is_discarded()) return false;
    nlohmann::json const* list = nullptr;
    if (j.is_array()) {
        list = &j;
    } else if (j.is_object()) {
        if (auto it = j.find("items"); it != j.end() && it->is_array()) list = &*it;
    }
    if (!list) return false;

    std::unordered_set<ItemKey, ItemKeyHash> seen;
    for (auto const& v : *list) {
        if (!v.is_string()) continue;
        auto key = ItemKey::parse(v.get_ref<std::string const&>());
        if (!key || key->empty() || !seen.insert(*key).second) continue;
        mItems.push_back(std::move(*key));
    }
    return true;
}

std::string Bookmarks::toJsonText() const {
    nlohmann::json j;
    j["version"] = FILE_VERSION;
    j["items"]   = nlohmann::json::array();
    for (auto const& k : mItems) j["items"].push_back(k.toString());
    return j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool Bookmarks::load(std::filesystem::path const& file) {
    mItems.clear();
    auto text = readFile(file);
    if (!text) return false;
    return fromJsonText(*text);
}

bool Bookmarks::save(std::filesystem::path const& file) const { return writeFileAtomic(file, toJsonText()); }

} // namespace bei::core
