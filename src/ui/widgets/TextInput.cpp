#include "ui/widgets/TextInput.h"

#include "game/Game.h"
#include "mod/Log.h"
#include "ui/UiRoot.h"
#include "ui/widgets/TextBox.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/thread/ClientThreadExecutor.h"

#include "mc/client/game/ClientInputCallbacks.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/input/KeyboardManager.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/input/enums/FocusImpact.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>

namespace bei::ui {

namespace {
TextBox*          gOwner     = nullptr; // focused box (client thread only)
bool              gKbClaimed = false;   // we hold KeyboardManager ownership
bool              gKbEnabled = false;   // ... and enabled the platform keyboard / IME
std::atomic<bool> gWantsText{false};    // gOwner != nullptr, readable from any thread
std::atomic<int>  gLoggedEvents{0};

auto const gStart = std::chrono::steady_clock::now();

/// Seconds a focused box may go undrawn before it is unfocused (screen closed, panel hidden).
constexpr double STALE_SECONDS = 0.5;

/// Decide synchronously (whatever thread the game calls us on) whether BEI takes the text;
/// the actual edit happens on the client thread, in order with the key events.
bool routeText(std::string const& text) {
    if (text.empty() || !gWantsText.load(std::memory_order_acquire)) return false;
    // A few samples help diagnose platform/IME behaviour (control characters shown escaped).
    if (gLoggedEvents.fetch_add(1, std::memory_order_relaxed) < 5) {
        std::string shown;
        for (unsigned char c : text)
            shown += c < 0x20 ? "\\x" + std::to_string(c) : std::string(1, static_cast<char>(c));
        log().info("Text input routed to BEI: \"{}\"", shown);
    }
    ll::thread::ClientThreadExecutor::getDefault().execute([text] { UiRoot::get().dispatchChar(text); });
    return true;
}

// Main entry of typed text (WM_CHAR, IME results, controller keyboards) before the scene stack.
LL_STATIC_HOOK(
    BeiTextCharHook,
    ll::memory::HookPriority::High,
    &ClientInputCallbacks::handleTextChar,
    void,
    ::IClientInstance& client,
    std::string const& text,
    ::FocusImpact      impact
) {
    if (routeText(text)) return;
    origin(client, text, impact);
}

// Fallback for paths that deliver text straight to the focused scene (e.g. buffered events).
LL_TYPE_INSTANCE_HOOK(
    BeiSceneTextCharHook,
    ll::memory::HookPriority::High,
    UIScene,
    &UIScene::$handleTextChar,
    void,
    std::string const& text,
    ::FocusImpact      impact
) {
    if (routeText(text)) return;
    origin(text, impact);
}

void claimKeyboard(TextBox const& box) {
    auto* client = game::currentClient();
    if (!client) return;
    auto& kb   = client->getKeyboardManager();
    gKbClaimed = kb.tryClaimKeyboardOwnership();
    if (!gKbClaimed) {
        log().warn("Could not claim the game keyboard for a text box");
        return;
    }
    // Start from an empty platform buffer: BEI owns the text and the selection, the platform
    // only supplies insertion events (seeding it would make it re-send stale text).
    gKbEnabled = kb.tryEnableKeyboard(
        std::string{},
        static_cast<int>(std::max<std::size_t>(box.maxLength, 1)),
        false,
        false,
        false,
        Vec2{box.rect.x, box.rect.bottom()},
        box.rect.h
    );
}

void releaseKeyboard() {
    if (!gKbClaimed && !gKbEnabled) return;
    if (auto* client = game::currentClient()) {
        auto& kb = client->getKeyboardManager();
        if (gKbEnabled) kb.disableKeyboard();
        if (gKbClaimed) kb.releaseKeyboardOwnership();
    }
    gKbClaimed = gKbEnabled = false;
}

/// Invisible layer whose visible() runs on every UiRoot frame: unfocuses a box that is no longer
/// drawn so the game keyboard is never left claimed.
class FocusWatchdog final : public IUiLayer {
public:
    [[nodiscard]] char const* name() const override { return "bei.text_input"; }
    [[nodiscard]] int         zOrder() const override { return -1000000; }
    [[nodiscard]] bool        visible(FrameInfo const&) const override {
        if (gOwner && TextInput::now() - gOwner->lastRenderTime() > STALE_SECONDS) {
            log().info("Focused text box is no longer drawn; unfocusing it");
            gOwner->setFocused(false);
        }
        if (!gOwner) releaseKeyboard(); // a focused box was destroyed (TextInput::forget)
        return false;
    }
    void render(Painter&, FrameInfo const&) override {}
};
FocusWatchdog gWatchdog;
BEI_REGISTER_UI_LAYER(gWatchdog);
} // namespace

void TextInput::install() {
    BeiTextCharHook::hook();
    BeiSceneTextCharHook::hook();
}

void TextInput::uninstall() {
    BeiSceneTextCharHook::unhook();
    BeiTextCharHook::unhook();
    if (gOwner) gOwner->setFocused(false);
    releaseKeyboard();
}

void TextInput::acquire(TextBox& box) {
    if (gOwner == &box) return;
    if (gOwner) gOwner->setFocused(false); // calls release() for the previous owner
    gOwner = &box;
    gWantsText.store(true, std::memory_order_release);
    claimKeyboard(box);
    log().info("Text box focused (keyboard claimed: {}, enabled: {})", gKbClaimed, gKbEnabled);
}

void TextInput::release(TextBox& box) {
    if (gOwner != &box) return;
    gOwner = nullptr;
    gWantsText.store(false, std::memory_order_release);
    releaseKeyboard();
    log().info("Text box unfocused");
}

void TextInput::forget(TextBox& box) noexcept {
    if (gOwner != &box) return;
    gOwner = nullptr;
    gWantsText.store(false, std::memory_order_release);
}

TextBox* TextInput::focusedBox() noexcept { return gOwner; }

double TextInput::now() noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - gStart).count();
}

} // namespace bei::ui
