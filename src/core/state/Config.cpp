#include "core/state/Config.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace bei::core {

namespace {
template <class T>
void read(nlohmann::json const& j, char const* key, T& field) {
    if (auto it = j.find(key); it != j.end()) {
        try {
            field = it->get<T>();
        } catch (...) {} // keep default on type mismatch
    }
}
} // namespace

bool Config::load(std::filesystem::path const& file) {
    *this = Config{};
    std::ifstream in(file);
    if (!in) return false;
    auto j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    read(j, "overlayEnabled", overlayEnabled);
    read(j, "bookmarksEnabled", bookmarksEnabled);
    read(j, "cheatMode", cheatMode);
    read(j, "showOnAllContainers", showOnAllContainers);
    read(j, "centerSearchBar", centerSearchBar);
    read(j, "maxColumns", maxColumns);
    read(j, "lastSearch", lastSearch);
    return true;
}

bool Config::save(std::filesystem::path const& file) const {
    nlohmann::json j{
        {"version",             version            },
        {"overlayEnabled",      overlayEnabled     },
        {"bookmarksEnabled",    bookmarksEnabled   },
        {"cheatMode",           cheatMode          },
        {"showOnAllContainers", showOnAllContainers},
        {"centerSearchBar",     centerSearchBar    },
        {"maxColumns",          maxColumns         },
        {"lastSearch",          lastSearch         },
    };
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::trunc);
    if (!out) return false;
    out << j.dump(2);
    return static_cast<bool>(out);
}

} // namespace bei::core
