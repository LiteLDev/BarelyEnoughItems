#include "core/model/ItemKey.h"

#include <charconv>

namespace bei::core {

std::string_view ItemKey::nameSpace() const noexcept {
    std::string_view v{id};
    auto             pos = v.find(':');
    return pos == std::string_view::npos ? std::string_view{"minecraft"} : v.substr(0, pos);
}

std::string_view ItemKey::path() const noexcept {
    std::string_view v{id};
    auto             pos = v.find(':');
    return pos == std::string_view::npos ? v : v.substr(pos + 1);
}

std::string ItemKey::toString() const {
    if (isWildcard()) return id + ":*";
    if (aux == 0) return id;
    return id + ":" + std::to_string(aux);
}

std::optional<ItemKey> ItemKey::parse(std::string_view text) {
    if (text.empty()) return std::nullopt;
    // Forms: "path", "ns:path", "ns:path:aux", "ns:path:*".
    auto first = text.find(':');
    if (first == std::string_view::npos) return ItemKey{"minecraft:" + std::string{text}, 0};
    auto second = text.find(':', first + 1);
    if (second == std::string_view::npos) {
        if (first == 0 || first + 1 == text.size()) return std::nullopt;
        return ItemKey{std::string{text}, 0};
    }
    auto idPart  = text.substr(0, second);
    auto auxPart = text.substr(second + 1);
    if (first == 0 || first + 1 == second || auxPart.empty()) return std::nullopt;
    if (auxPart == "*") return ItemKey{std::string{idPart}, ANY_AUX};
    int  aux = 0;
    auto res = std::from_chars(auxPart.data(), auxPart.data() + auxPart.size(), aux);
    if (res.ec != std::errc{} || res.ptr != auxPart.data() + auxPart.size() || aux < -32768 || aux > 32767) {
        return std::nullopt;
    }
    return ItemKey{std::string{idPart}, static_cast<std::int16_t>(aux)};
}

} // namespace bei::core
