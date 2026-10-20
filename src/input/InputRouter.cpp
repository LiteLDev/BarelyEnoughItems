// Scaffold implementation: mouse routing, raw R/U keys on BEI's own panels, key routing to
// a focused text box. No KeyRegistry, no vanilla slot hover, no text-char hook.
// Unit "Input routing" completes it.
#include "input/InputRouter.h"

#include "app/Controller.h"
#include "game/Game.h"
#include "ui/UiRoot.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/thread/ClientThreadExecutor.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/deps/input/HIDController.h"
#include "mc/deps/input/MouseAction.h"

namespace bei::input {

namespace {
ll::event::ListenerPtr gMouse;
ll::event::ListenerPtr gKey;

float invGuiScale() {
    auto* client = game::currentClient();
    if (!client) return 1.f;
    return client->getGuiData()->mInvGuiScale;
}

void onClient(std::function<void()> fn) { ll::thread::ClientThreadExecutor::getDefault().execute(std::move(fn)); }
} // namespace

InputRouter& InputRouter::get() {
    static InputRouter instance;
    return instance;
}

void InputRouter::install() {
    auto& bus = ll::event::EventBus::getInstance();
    gMouse    = bus.emplaceListener<ll::event::MouseInputEvent>([](ll::event::MouseInputEvent& ev) {
        float s      = invGuiScale();
        float x      = ev.x() * s;
        float y      = ev.y() * s;
        auto  action = static_cast<int>(ev.actionButtonId());
        auto& root   = ui::UiRoot::get();
        if (action == MouseAction::ActionMove || action == MouseAction::ActionMoveRelative) {
            if (action == MouseAction::ActionMove) onClient([x, y] { ui::UiRoot::get().setMouse(x, y); });
            return;
        }
        if (action == MouseAction::ActionWheel) {
            if (!root.contains(x, y)) return;
            float delta = static_cast<float>(ev.buttonData());
            onClient([x, y, delta] { ui::UiRoot::get().dispatchScroll(x, y, delta); });
            ev.cancel();
            return;
        }
        ui::MouseButton button;
        switch (action) {
        case MouseAction::ActionLeft: button = ui::MouseButton::Left; break;
        case MouseAction::ActionRight: button = ui::MouseButton::Right; break;
        case MouseAction::ActionMiddle: button = ui::MouseButton::Middle; break;
        default: return;
        }
        bool down = ev.buttonData() == MouseAction::DataDown;
        bool over = root.contains(x, y) || root.wantsTextInput();
        onClient([=] { ui::UiRoot::get().dispatchMouseButton(button, down, x, y); });
        if (over && root.contains(x, y)) ev.cancel();
    });
    gKey = bus.emplaceListener<ll::event::KeyInputEvent>([](ll::event::KeyInputEvent& ev) {
        int   vk   = ev.keyCode();
        bool  down = ev.isDown();
        auto& root = ui::UiRoot::get();
        if (root.wantsTextInput()) {
            onClient([vk, down] { ui::UiRoot::get().dispatchKey(vk, down); });
            ev.cancel();
            return;
        }
        if (!down || ev.controller().mTextboxIsFocused) return;
        if (vk == 'R' || vk == 'U') {
            onClient([vk] {
                auto& c = app::Controller::get();
                if (auto item = c.hoveredItem()) {
                    if (vk == 'R') c.showRecipes(*item);
                    else c.showUses(*item);
                }
            });
        }
    });
}

void InputRouter::uninstall() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto* l : {&gMouse, &gKey}) {
        if (*l) bus.removeListener(*l);
        l->reset();
    }
}

} // namespace bei::input
