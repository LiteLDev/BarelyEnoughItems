#include "mod/BEI.h"

#include "actions/Actions.h"
#include "app/Controller.h"
#include "app/I18n.h"
#include "game/items/ItemCatalog.h"
#include "game/recipes/RecipeManager.h"
#include "input/InputRouter.h"
#include "mod/Log.h"
#include "ui/UiRoot.h"
#include "ui/recipe/RecipeScreen.h"

#include "ll/api/mod/RegisterHelper.h"

namespace bei {

ll::io::Logger& log() { return BEI::getInstance().getSelf().getLogger(); }

BEI& BEI::getInstance() {
    static BEI instance;
    return instance;
}

bool BEI::load() {
    // Flush every message so the e2e harness (and people tailing logs) see them immediately.
    getSelf().getLogger().setFlushLevel(ll::io::LogLevel::Trace);
    app::loadTranslations(getSelf().getModDir() / "lang");
    app::Controller::get().load(getSelf().getConfigDir());
    return true;
}

bool BEI::enable() {
    game::ItemCatalog::get().install();
    game::RecipeManager::get().install();
    ui::UiRoot::get().install();
    ui::RecipeScreen::get().install();
    input::InputRouter::get().install();
    actions::Actions::get().install();
    log().info("BEI enabled");
    return true;
}

bool BEI::disable() {
    actions::Actions::get().uninstall();
    input::InputRouter::get().uninstall();
    ui::RecipeScreen::get().uninstall();
    ui::UiRoot::get().uninstall();
    game::RecipeManager::get().uninstall();
    game::ItemCatalog::get().uninstall();
    app::Controller::get().save();
    return true;
}

} // namespace bei

LL_REGISTER_MOD(bei::BEI, bei::BEI::getInstance());
