#include "core/state/Config.h"

#include "core/state/AtomicWrite.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string_view>

namespace bei::core {

namespace {

using json = nlohmann::json;

// Strictly typed readers: a value of the wrong JSON type leaves the field untouched.
void read(json const& j, char const* key, bool& field) {
    if (auto it = j.find(key); it != j.end() && it->is_boolean()) field = it->get<bool>();
}

void read(json const& j, char const* key, int& field) {
    auto it = j.find(key);
    if (it == j.end()) return;
    if (it->is_number_integer()) {
        // Covers both signed and unsigned storage; reject values that don't fit an int.
        if (it->is_number_unsigned()) {
            auto v = it->get<std::uint64_t>();
            if (v <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) field = static_cast<int>(v);
        } else {
            auto v = it->get<std::int64_t>();
            if (v >= std::numeric_limits<int>::min() && v <= std::numeric_limits<int>::max())
                field = static_cast<int>(v);
        }
    } else if (it->is_number_float()) {
        // Accept integral floats ("3.0"), which some editors produce.
        double d = it->get<double>();
        if (std::isfinite(d) && d == std::floor(d) && d >= std::numeric_limits<int>::min()
            && d <= std::numeric_limits<int>::max()) {
            field = static_cast<int>(d);
        }
    }
}

void read(json const& j, char const* key, std::string& field) {
    if (auto it = j.find(key); it != j.end() && it->is_string()) field = it->get<std::string>();
}

/// Array of strings; non-string elements are skipped, a non-array leaves the field untouched.
void read(json const& j, char const* key, std::vector<std::string>& field) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_array()) return;
    field.clear();
    for (auto const& e : *it) {
        if (e.is_string()) field.push_back(e.get<std::string>());
    }
}

/// Keys read by fromJsonText (plus accepted aliases). Everything else is preserved in extraFields.
constexpr std::string_view KNOWN_KEYS[] = {
    "version",
    "overlayEnabled",
    "bookmarksEnabled",
    "cheatMode",
    "showOnAllContainers",
    "centerSearchBar",
    "maxColumns",
    "lastSearch",
    "cheatGiveStackSize",
    "showItemIds",
    "showModName",
    "bookmarksAddToFront",
    "rememberPage",
    "lastPage",
    "searchHistory",
    "itemPanelColumns",
    "columns",
};

/// Upgrade a config document from `fromVersion` to Config::CURRENT_VERSION in place.
/// Add a `case N:` step for each format change (N = the version being upgraded from).
void migrate(json& j, int fromVersion) {
    // Tolerate a few alias names for maxColumns regardless of version.
    if (!j.contains("maxColumns")) {
        for (char const* alias : {"itemPanelColumns", "columns"}) {
            if (auto it = j.find(alias); it != j.end()) {
                j["maxColumns"] = *it;
                break;
            }
        }
    }
    for (int v = std::max(fromVersion, 0); v < Config::CURRENT_VERSION; ++v) {
        switch (v) {
        case 0: // Pre-release files without a version field: same layout as version 1.
        default:
            break;
        }
    }
}

/// Truncate to at most `max` bytes without splitting a UTF-8 sequence.
void truncateUtf8(std::string& s, std::size_t max) {
    if (s.size() <= max) return;
    std::size_t n = max;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    s.resize(n);
}

bool isBlank(std::string_view s) { return s.find_first_not_of(" \t\r\n") == std::string_view::npos; }

} // namespace

void Config::sanitize() {
    maxColumns         = std::clamp(maxColumns, 0, MAX_COLUMNS);
    cheatGiveStackSize = std::clamp(cheatGiveStackSize, 1, MAX_STACK_SIZE);
    lastPage           = std::max(lastPage, 0);
    truncateUtf8(lastSearch, MAX_SEARCH_LENGTH);
    // searchHistory: truncated, non-blank, distinct (first occurrence wins), capped.
    std::vector<std::string> history;
    for (auto& entry : searchHistory) {
        if (history.size() >= MAX_SEARCH_HISTORY) break;
        truncateUtf8(entry, MAX_SEARCH_LENGTH);
        if (isBlank(entry) || std::find(history.begin(), history.end(), entry) != history.end()) continue;
        history.push_back(std::move(entry));
    }
    searchHistory = std::move(history);
}

bool Config::pushSearchHistory(std::string_view text) {
    std::string entry{text};
    truncateUtf8(entry, MAX_SEARCH_LENGTH);
    if (isBlank(entry)) return false;
    if (!searchHistory.empty() && searchHistory.front() == entry) return false;
    std::erase(searchHistory, entry);
    searchHistory.insert(searchHistory.begin(), std::move(entry));
    if (searchHistory.size() > MAX_SEARCH_HISTORY) searchHistory.resize(MAX_SEARCH_HISTORY);
    return true;
}

bool Config::fromJsonText(std::string_view text) {
    *this  = Config{};
    auto j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;

    int fileVersion = 0;
    read(j, "version", fileVersion);
    migrate(j, fileVersion);

    read(j, "overlayEnabled", overlayEnabled);
    read(j, "bookmarksEnabled", bookmarksEnabled);
    read(j, "cheatMode", cheatMode);
    read(j, "showOnAllContainers", showOnAllContainers);
    read(j, "centerSearchBar", centerSearchBar);
    read(j, "maxColumns", maxColumns);
    read(j, "lastSearch", lastSearch);
    read(j, "cheatGiveStackSize", cheatGiveStackSize);
    read(j, "showItemIds", showItemIds);
    read(j, "showModName", showModName);
    read(j, "bookmarksAddToFront", bookmarksAddToFront);
    read(j, "rememberPage", rememberPage);
    read(j, "lastPage", lastPage);
    read(j, "searchHistory", searchHistory);
    sanitize();

    // Keep fields this build doesn't know (e.g. written by a newer BEI) so save() doesn't drop them.
    json extra = json::object();
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (std::find(std::begin(KNOWN_KEYS), std::end(KNOWN_KEYS), it.key()) == std::end(KNOWN_KEYS)) {
            extra[it.key()] = it.value();
        }
    }
    if (!extra.empty()) extraFields = extra.dump(-1, ' ', false, json::error_handler_t::replace);
    version = std::max(fileVersion, CURRENT_VERSION);
    return true;
}

std::string Config::toJsonText() const {
    Config c = *this;
    c.sanitize();
    json j{
        {"version", std::max(c.version, CURRENT_VERSION)},
        {"overlayEnabled", c.overlayEnabled},
        {"bookmarksEnabled", c.bookmarksEnabled},
        {"cheatMode", c.cheatMode},
        {"showOnAllContainers", c.showOnAllContainers},
        {"centerSearchBar", c.centerSearchBar},
        {"maxColumns", c.maxColumns},
        {"lastSearch", c.lastSearch},
        {"cheatGiveStackSize", c.cheatGiveStackSize},
        {"showItemIds", c.showItemIds},
        {"showModName", c.showModName},
        {"bookmarksAddToFront", c.bookmarksAddToFront},
        {"rememberPage", c.rememberPage},
        {"lastPage", c.lastPage},
        {"searchHistory", c.searchHistory},
    };
    if (!extraFields.empty()) {
        auto extra = json::parse(extraFields, nullptr, false);
        if (extra.is_object()) {
            for (auto it = extra.begin(); it != extra.end(); ++it) {
                if (!j.contains(it.key())) j[it.key()] = it.value();
            }
        }
    }
    return j.dump(2, ' ', false, json::error_handler_t::replace);
}

bool Config::load(std::filesystem::path const& file) {
    *this     = Config{};
    auto text = readFile(file);
    if (!text) return false;
    return fromJsonText(*text);
}

bool Config::save(std::filesystem::path const& file) const { return writeFileAtomic(file, toJsonText()); }

} // namespace bei::core
