#include "app/I18n.h"

#include "mod/Log.h"

#include "ll/api/i18n/I18n.h"

#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"

namespace bei::app {

namespace {
ll::i18n::I18n& table() {
    static ll::i18n::I18n instance; // BEI's own table, independent of other mods
    return instance;
}

std::string currentLocale() {
    try {
        return getI18n().getCurrentLanguage()->getFullLanguageCode();
    } catch (...) {
        return "en_US";
    }
}
} // namespace

void loadTranslations(std::filesystem::path const& langDir) {
    if (auto res = table().load(langDir); !res) log().warn("Failed to load translations from {}", langDir.string());
}

std::string tr(std::string_view key, std::string_view fallback) {
    auto value = table().get(key, currentLocale());
    if (value == key) value = table().get(key, "en_US");
    if (value == key) return std::string{fallback.empty() ? key : fallback};
    return std::string{value};
}

} // namespace bei::app
