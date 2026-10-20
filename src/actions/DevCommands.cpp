// Developer-only client command for exercising the Actions API before its UI callers exist.
// Compiled only with -DBEI_DEV (off by default), e.g.:
//   xmake f -a x64 -m release -p windows --target_type=client --cxflags=-DBEI_DEV -y
//
//   /beidebug cheat                      log canCheat()
//   /beidebug give <id[:aux]> [count]    Actions::give
//   /beidebug transfer <recipe> [count]  queue a transfer (count > 1 = max sets); it runs as soon
//                                        as a crafting screen is open. recipe: planks|sticks|table|chest
//   /beidebug check <recipe>             queue a checkTransfer and log the result
//   /beidebug delete                     queue deleteCursorItem; runs when an item is on the cursor
#ifdef BEI_DEV

#include "actions/Actions.h"
#include "game/Game.h"
#include "mod/Log.h"


#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/command/ClientCommandRegisterEvent.h"
#include "ll/api/thread/ClientThreadExecutor.h"

#include "mc/server/commands/CommandOutput.h"

#include <chrono>
#include <format>
#include <functional>
#include <optional>
#include <string>

namespace bei::actions::dev {

// Command parameters need external linkage for LeviLamina's reflection.
struct DebugParams {
    std::string action;
    std::string arg;
    int         count = 1;
};

namespace {

using Params = DebugParams;

ll::event::ListenerPtr gRegisterListener;

core::IngredientView ing(char const* id) { return {{core::ItemKey{id, core::ItemKey::ANY_AUX}}, 1}; }

std::optional<core::RecipeView> recipeNamed(std::string const& name) {
    core::RecipeView r;
    r.category = core::category::CRAFTING;
    r.id       = "beidebug:" + name;
    if (name == "planks") {
        r.shapeless = true;
        r.inputs    = {ing("minecraft:oak_log")};
        r.outputs   = {
            {{core::ItemKey{"minecraft:oak_planks", 0}}, 4}
        };
    } else if (name == "sticks") {
        r.width = 1, r.height = 2;
        r.inputs  = {ing("minecraft:oak_planks"), ing("minecraft:oak_planks")};
        r.outputs = {
            {{core::ItemKey{"minecraft:stick", 0}}, 4}
        };
    } else if (name == "table") {
        r.width = 2, r.height = 2;
        r.inputs.assign(4, ing("minecraft:oak_planks"));
        r.outputs = {
            {{core::ItemKey{"minecraft:crafting_table", 0}}, 1}
        };
    } else if (name == "chest") {
        r.width = 3, r.height = 3;
        r.inputs.assign(9, ing("minecraft:oak_planks"));
        r.inputs[4] = {};
        r.outputs   = {
            {{core::ItemKey{"minecraft:chest", 0}}, 1}
        };
    } else {
        return std::nullopt;
    }
    return r;
}

std::string describe(TransferCheck const& c) {
    std::string missing;
    for (int i : c.missingInputs) missing += (missing.empty() ? "" : ",") + std::to_string(i);
    return std::format("possible={} missing=[{}] canCraft={}", c.possible, missing, c.canCraft());
}

/// Poll on the client thread until `ready()` (or ~20 s), then run `fn` after a short settle delay.
void whenReady(std::function<bool()> ready, std::function<void()> fn, int triesLeft = 80) {
    using namespace std::chrono_literals;
    ll::thread::ClientThreadExecutor::getDefault().executeAfter(
        [ready = std::move(ready), fn = std::move(fn), triesLeft]() mutable {
            if (ready()) {
                ll::thread::ClientThreadExecutor::getDefault().executeAfter(std::move(fn), 500ms);
            } else if (triesLeft > 0) {
                whenReady(std::move(ready), std::move(fn), triesLeft - 1);
            } else {
                log().warn(
                    "beidebug: timed out waiting (grid={}, cursor={}, player={})",
                    Actions::get().currentCraftingGridSize(),
                    Actions::get().hasCursorItem(),
                    static_cast<void*>(game::currentPlayer())
                );
            }
        },
        250ms
    );
}

void run(Params const& p, CommandOutput& output) {
    auto& actions = Actions::get();
    if (p.action == "cheat") {
        log().info("beidebug: canCheat={}", actions.canCheat());
        output.success("canCheat={}", actions.canCheat());
    } else if (p.action == "give") {
        auto key = core::ItemKey::parse(p.arg);
        bool ok  = key && actions.give(*key, p.count);
        log().info("beidebug: give {} {} -> {}", p.arg, p.count, ok);
        output.success("give -> {}", ok);
    } else if (p.action == "transfer" || p.action == "check") {
        auto recipe = recipeNamed(p.arg);
        if (!recipe) {
            output.error("unknown recipe {}", p.arg);
            return;
        }
        bool const transfer = p.action == "transfer";
        bool const max      = p.count > 1;
        output.success("queued {} {}", p.action, p.arg);
        whenReady(
            [] { return Actions::get().currentCraftingGridSize() > 0; },
            [recipe = *recipe, transfer, max] {
                auto& a = Actions::get();
                log().info(
                    "beidebug: grid={} check {}: {}",
                    a.currentCraftingGridSize(),
                    recipe.id,
                    describe(a.checkTransfer(recipe))
                );
                if (transfer) log().info("beidebug: transfer {} max={} -> {}", recipe.id, max, a.transfer(recipe, max));
            }
        );
    } else if (p.action == "delete") {
        output.success("queued delete");
        whenReady(
            [] { return Actions::get().hasCursorItem(); },
            [] {
                auto& a = Actions::get();
                log().info("beidebug: canDelete={}", a.canDeleteCursorItem());
                bool const deleted = a.deleteCursorItem();
                log().info("beidebug: delete -> {} (cursor now {})", deleted, a.hasCursorItem() ? "full" : "empty");
            }
        );
    } else {
        output.error("unknown action {}", p.action);
    }
}

void registerCommand() {
    auto& cmd =
        ll::command::CommandRegistrar::getInstance(true).getOrCreateCommand("beidebug", "BEI developer actions");
    cmd.overload<Params>().required("action").optional("arg").optional("count").execute(
        [](CommandOrigin const&, CommandOutput& output, Params const& p) { run(p, output); }
    );
    log().info("beidebug: dev command registered");
}

} // namespace

void install() {
    gRegisterListener =
        ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientCommandRegisterEvent>([](auto&) {
            registerCommand();
        });
}

void uninstall() {
    if (gRegisterListener) ll::event::EventBus::getInstance().removeListener(gRegisterListener);
    gRegisterListener.reset();
}

} // namespace bei::actions::dev

#endif // BEI_DEV
