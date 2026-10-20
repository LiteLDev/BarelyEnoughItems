#include "core/state/Bookmarks.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace bei::core {

bool Bookmarks::contains(ItemKey const& item) const {
    return std::find(mItems.begin(), mItems.end(), item) != mItems.end();
}

void Bookmarks::add(ItemKey const& item) {
    if (item.empty() || contains(item)) return;
    mItems.insert(mItems.begin(), item);
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

bool Bookmarks::load(std::filesystem::path const& file) {
    mItems.clear();
    std::ifstream in(file);
    if (!in) return false;
    auto j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("items") || !j["items"].is_array()) return false;
    for (auto const& v : j["items"]) {
        if (!v.is_string()) continue;
        if (auto key = ItemKey::parse(v.get<std::string>()); key && !contains(*key)) mItems.push_back(*key);
    }
    return true;
}

bool Bookmarks::save(std::filesystem::path const& file) const {
    nlohmann::json j;
    j["version"] = 1;
    j["items"]   = nlohmann::json::array();
    for (auto const& k : mItems) j["items"].push_back(k.toString());
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::trunc);
    if (!out) return false;
    out << j.dump(2);
    return static_cast<bool>(out);
}

} // namespace bei::core
