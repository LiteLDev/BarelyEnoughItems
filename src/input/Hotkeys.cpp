// BEI hotkeys (R recipes, U uses, A bookmark, O overlay, Backspace back).
//
// Keys are registered with LeviLamina's KeyRegistry so they appear in Settings > Keyboard &
// Mouse, can be remapped there, and are saved with the game's options. Their names come from
// assets/lang/<locale>/input.json ("bei.key.<name>"), injected into the game's I18n.
//
// Detection does not use KeyRegistry's button handlers: on this build a key that already has
// a binding in the UI ("screen") input mapping (A, Backspace, ...) never reaches a second
// binding, so BEI's handler would silently not fire. Instead the currently mapped key codes
// are read from the game's keyboard layout whenever the controls are (re)built, and the raw
// KeyInputEvent stream (window thread) is matched against them. BEI's keymappings are added to
// no input mapping at all, so they never steal or shadow a vanilla binding, and they are
// marked as shared keys so the layout does not unassign a default that a vanilla action
// (e.g. A = strafe left) also uses.
#include "input/InputState.h"

#include "app/Controller.h"
#include "app/I18n.h"
#include "mod/Log.h"
#include "ui/UiRoot.h"
#include "ui/overlay/ItemListPanel.h"
#include "ui/recipe/RecipeScreen.h"

#include "ll/api/input/KeyRegistry.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/input/ClientInputMappingFactory.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/input/RemappingLayout.h"
#include "mc/client/input/VanillaClientInputMappingFactory.h"
#include "mc/locale/I18nImpl.h"
#include "mc/locale/Localization.h"

#include <array>
#include <atomic>
#include <string>
#include <string_view>
#include <vector>

// Avoid <windows.h> here (macro clashes with game headers); only this one call is needed.
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
#pragma comment(lib, "user32")

namespace bei::input::detail {

namespace {

enum class Action { ShowRecipes, ShowUses, ToggleBookmark, ToggleOverlay, Back, FocusSearch };

struct HotkeyDef {
    std::string_view name;       ///< KeyRegistry name; the game calls it "key.BEI.<name>".
    int              defaultKey; ///< Windows virtual-key code.
    Action           action;
    /// Fires only while Ctrl is held. The game's key bindings are single keys, so the modifier
    /// is fixed and only the key is remappable (the name in the controls menu says "Ctrl+").
    bool ctrl = false;
};

constexpr int VK_BACKSPACE = 0x08;
constexpr int VK_CONTROL_  = 0x11;

constexpr std::array<HotkeyDef, 6> HOTKEYS{
    {
     {"show_recipes", 'R', Action::ShowRecipes},
     {"show_uses", 'U', Action::ShowUses},
     {"toggle_bookmark", 'A', Action::ToggleBookmark},
     {"toggle_overlay", 'O', Action::ToggleOverlay},
     {"back", VK_BACKSPACE, Action::Back},
     {"focus_search", 'F', Action::FocusSearch, true}, // JEI: Ctrl+F
    }
};

/// Keymapping action ids are "key.<modName>.<name>"; BEI's mod name is "BEI".
constexpr std::string_view KEY_PREFIX = "key.BEI.";

/// Up to two keys per hotkey, as currently mapped in the game's keyboard layout (0 = none).
/// Written on the client thread, read on the window thread.
constexpr std::size_t                                                     KEYS_PER_HOTKEY = 2;
std::array<std::array<std::atomic<int>, KEYS_PER_HOTKEY>, HOTKEYS.size()> gKeys{};

std::atomic<bool> gActive{false};

void resetToDefaults() {
    for (std::size_t i = 0; i < HOTKEYS.size(); ++i) {
        gKeys[i][0] = HOTKEYS[i].defaultKey;
        for (std::size_t k = 1; k < KEYS_PER_HOTKEY; ++k) gKeys[i][k] = 0;
    }
}

/// Client thread: the conditions under which a hotkey acts. Hotkeys only act on screens with
/// a cursor (inventory, containers, the recipe screen), never during gameplay where the same
/// letters are movement/game keys, and never while a BEI text box has focus. Vanilla text
/// focus was checked when the key event arrived.
bool hotkeysAllowed() {
    auto& snap = snapshot();
    return gActive.load() && snap.inLevel.load() && snap.cursorScreen.load() && !ui::UiRoot::get().wantsTextInput();
}

void run(HotkeyDef const& def) {
    if (!hotkeysAllowed()) return;
    auto& controller = app::Controller::get();
    switch (def.action) {
    case Action::ToggleOverlay:
        controller.toggleOverlay();
        log().info("Hotkey {}: item list {}", def.name, controller.config().overlayEnabled ? "shown" : "hidden");
        return;
    case Action::Back:
        if (!ui::RecipeScreen::get().isOpen()) return;
        log().info("Hotkey {}", def.name);
        controller.back();
        return;
    case Action::FocusSearch:
        if (ui::RecipeScreen::get().isOpen()) return;
        log().info("Hotkey {}: search box {}", def.name, ui::ItemListPanel::get().focusSearch() ? "focused" : "not shown");
        return;
    default:
        break;
    }
    auto item = controller.hoveredItem();
    if (!item) return;
    log().info("Hotkey {} on {}", def.name, item->toString());
    switch (def.action) {
    case Action::ShowRecipes:
        controller.showRecipes(*item);
        break;
    case Action::ShowUses:
        controller.showUses(*item);
        break;
    case Action::ToggleBookmark:
        controller.toggleBookmark(*item);
        break;
    default:
        break;
    }
}

/// Client thread: refresh gKeys from the (possibly remapped) keyboard layout.
void readLayout(RemappingLayout const& layout) {
    for (std::size_t i = 0; i < HOTKEYS.size(); ++i) {
        auto        action  = std::string{KEY_PREFIX} + std::string{HOTKEYS[i].name};
        auto const& mapping = layout.getKeymappingByAction(action);
        // Not in the layout (keys registered too late): keep the current/default keys.
        if (*mapping.mAction != action) continue;
        std::array<int, KEYS_PER_HOTKEY> keys{};
        std::size_t                      n = 0;
        // Mouse buttons are "alt keys" (negative codes); BEI hotkeys are keyboard-only.
        if (mapping.isAssigned() && !mapping.isAltKey()) {
            for (int key : *mapping.mKeys) {
                if (key > 0 && n < KEYS_PER_HOTKEY) keys[n++] = key;
            }
        }
        for (std::size_t k = 0; k < KEYS_PER_HOTKEY; ++k) gKeys[i][k] = keys[k];
    }
}

} // namespace

// --- Hooks ------------------------------------------------------------------------------------

// Mark BEI's default keymappings as shared before the game de-duplicates its layout: R/U/O are
// free in vanilla, but A is strafe-left and must stay assigned to both actions.
LL_TYPE_INSTANCE_HOOK(
    HotkeySharedDefaultsHook,
    ll::memory::HookPriority::Low, // inside LeviLamina's hook, which appends the mod keys
    RemappingLayout,
    &RemappingLayout::assignDefaultMapping,
    void,
    std::vector<::Keymapping>&& newDefaultMapping
) {
    for (auto& mapping : newDefaultMapping) {
        if (std::string_view{*mapping.mAction}.starts_with(KEY_PREFIX)) mapping.mIsSharedKey = true;
    }
    origin(std::move(newDefaultMapping));
}

// Runs whenever the keyboard controls are (re)built, including after the player remaps a key.
LL_TYPE_INSTANCE_HOOK(
    HotkeyLayoutHook,
    ll::memory::HookPriority::Normal,
    VanillaClientInputMappingFactory,
    &VanillaClientInputMappingFactory::$_updateKeyboardAndMouseControls,
    void,
    IOptionRegistry& options
) {
    origin(options);
    if (auto layout = mKeyboardRemappingLayout.lock()) readLayout(*layout);
}

// The controls menu localizes "key.BEI.<name>"; answer from BEI's own translation table.
LL_TYPE_INSTANCE_HOOK(
    KeyNameI18nHook,
    ll::memory::HookPriority::Normal,
    I18nImpl,
    &I18nImpl::$get,
    std::string,
    std::string const&                          id,
    std::vector<std::string> const&             params,
    std::shared_ptr<::Localization const> const locale
) {
    // The menu asks for "key.BEI.<name>.name", "key.BEI.<name>.description" and "key.BEI.<name>".
    if (id.size() > KEY_PREFIX.size() && id.starts_with(KEY_PREFIX)) {
        constexpr std::string_view NAME_SUFFIX = ".name";
        constexpr std::string_view DESC_SUFFIX = ".description";
        auto                       rest        = std::string_view{id}.substr(KEY_PREFIX.size());
        if (rest.ends_with(DESC_SUFFIX)) return app::tr("bei.key." + std::string{rest}, " ");
        if (rest.ends_with(NAME_SUFFIX)) rest.remove_suffix(NAME_SUFFIX.size());
        return app::tr("bei.key." + std::string{rest}, rest);
    }
    return origin(id, params, locale);
}

namespace {
bool gLayoutHooked = false; // the layout hooks must be live from BEI::load, before enable

void hookLayout() {
    if (gLayoutHooked) return;
    HotkeySharedDefaultsHook::hook();
    HotkeyLayoutHook::hook();
    gLayoutHooked = true;
}

void unhookLayout() {
    if (!gLayoutHooked) return;
    HotkeyLayoutHook::unhook();
    HotkeySharedDefaultsHook::unhook();
    gLayoutHooked = false;
}
} // namespace

void registerHotkeys() {
    // The game reads registered keys into its keyboard layout before mods are enabled, so this
    // must run during BEI::load. Keys are created once per process (KeyRegistry keys cannot be
    // removed).
    hookLayout();
    static bool const registered = [] {
        resetToDefaults();
        auto& registry = ll::input::KeyRegistry::getInstance();
        for (auto const& def : HOTKEYS) {
            auto& handle = registry.getOrCreateKey(def.name, {def.defaultKey}, true);
            handle.setInputMappingStack({}); // listed and remappable, but bound in no mapping
        }
        return true;
    }();
    (void)registered;
}

void installHotkeys() {
    registerHotkeys(); // no-op when BEI::load already did it
    KeyNameI18nHook::hook();
    gActive = true;
}

void uninstallHotkeys() {
    gActive = false;
    KeyNameI18nHook::unhook();
    unhookLayout();
}

void onHotkeyKeyDown(int vk) {
    if (!gActive.load() || vk <= 0) return;
    auto& snap = snapshot();
    // Cheap pre-checks; run() re-checks on the client thread.
    if (!snap.cursorScreen.load() || snap.vanillaTextFocus.load() || snap.beiTextFocus.load()) return;
    bool const ctrl = (GetAsyncKeyState(VK_CONTROL_) & 0x8000) != 0;
    for (std::size_t i = 0; i < HOTKEYS.size(); ++i) {
        if (HOTKEYS[i].ctrl && !ctrl) continue;
        for (auto const& key : gKeys[i]) {
            if (key.load() != vk) continue;
            post([i] { run(HOTKEYS[i]); });
            break;
        }
    }
}

} // namespace bei::input::detail
