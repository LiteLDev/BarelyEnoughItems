#include "ui/recipe/RecipeLayer.h"

#include "actions/Actions.h"
#include "app/Controller.h"
#include "app/I18n.h"
#include "game/items/ItemCatalog.h"
#include "game/recipes/RecipeManager.h"
#include "ui/UiRoot.h"
#include "ui/gfx/Painter.h"
#include "ui/widgets/Tooltip.h"

#include <algorithm>
#include <cmath>
#include <string>

#pragma comment(lib, "user32.lib")
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);

namespace bei::ui::recipe {

namespace {
constexpr float MARGIN = 4;  ///< Free space around the whole GUI.
constexpr float TAB_W  = 22;
constexpr float TAB_H  = 24;
constexpr float PAD    = 6;  ///< Panel inner padding.
constexpr float ROW_H  = 12; ///< Header rows (category title, page).
constexpr float BTN    = 11; ///< Header arrow / close buttons.
constexpr float PLUS   = 12; ///< Transfer ("+") button.
constexpr float STRIP  = 24; ///< Workstation strip width left of the panel.
constexpr float MIN_W  = 200;

constexpr int VK_BACK_   = 0x08;
constexpr int VK_ESCAPE_ = 0x1B;
constexpr int VK_PRIOR_  = 0x21;
constexpr int VK_NEXT_   = 0x22;
constexpr int VK_LEFT_   = 0x25;
constexpr int VK_UP_     = 0x26;
constexpr int VK_RIGHT_  = 0x27;
constexpr int VK_DOWN_   = 0x28;
constexpr int VK_SHIFT_  = 0x10;

constexpr Color ROW_BG     = Color::rgb(0x8B8B8B);
constexpr Color TAB_BG     = Color::rgb(0xA0A0A0);
constexpr Color SEPARATOR  = Color::rgb(0xADADAD);
constexpr Color BTN_BG     = Color::rgb(0x6F6F6F);
constexpr Color BTN_HOVER  = Color::rgb(0x7F89BF);
constexpr Color BTN_EDGE   = Color::rgb(0x000000);
constexpr Color PLUS_OK    = Color::rgb(0x4F8F3F);
constexpr Color PLUS_MISS  = Color::rgb(0x9F3F3F);

bool shiftHeld() { return (GetAsyncKeyState(VK_SHIFT_) & 0x8000) != 0; }

std::string categoryTitle(core::CategoryInfo const& info) {
    std::string fallback = info.fallbackTitle.empty() ? info.id : info.fallbackTitle;
    if (info.titleKey.empty()) return fallback;
    return app::tr(info.titleKey, fallback);
}

void button(Painter& p, core::Rect const& r, bool hover, Color bg = BTN_BG) {
    p.fillRect(r, BTN_EDGE);
    p.fillRect({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, hover ? BTN_HOVER : bg);
}

void closeGlyph(Painter& p, core::Rect const& r, bool hover) {
    Color c = hover ? Color::rgb(0xFFFFA0) : palette::TEXT_LIGHT;
    for (int i = 0; i < 5; ++i) {
        auto f = static_cast<float>(i);
        p.fillRect({r.x + 3 + f, r.y + 3 + f, 1, 1}, c, Painter::Layer::Foreground);
        p.fillRect({r.x + 7 - f, r.y + 3 + f, 1, 1}, c, Painter::Layer::Foreground);
    }
}

bool hit(core::Rect const& r, float x, float y) { return !r.empty() && r.contains(x, y); }
} // namespace

RecipeLayer& RecipeLayer::get() {
    static RecipeLayer instance;
    return instance;
}

// --- State ---------------------------------------------------------------------------------

void RecipeLayer::open(core::RecipeFocus const& focus) {
    mFocus      = focus;
    mGeneration = ~0ull;
    mInitial    = true;
    mTabs.clear();
    mChecks.clear();
    mSlots.clear();
    mHints.clear();
    mPlus.clear();
    mTabHits.clear();
}

void RecipeLayer::reset() {
    open({});
    mRendering  = false;
    mHasContent = false;
}

bool RecipeLayer::refresh() {
    auto& manager = game::RecipeManager::get();
    auto& index   = manager.index(); // may rebuild
    if (manager.generation() == mGeneration && !mTabs.empty()) return true;

    bool        initial  = mInitial || mTabs.empty();
    std::string keep     = initial ? mFocus.category : mTabs[static_cast<std::size_t>(mSel)].info.id;
    int         keepPage = initial ? mFocus.page : mPage;
    int         keepSel  = initial ? mFocus.categoryTab : mSel;

    auto makeTab = [&](std::string const& id, std::vector<std::size_t> recipes) {
        Tab tab;
        if (auto const* info = index.findCategory(id)) tab.info = *info;
        else tab.info = {.id = id, .fallbackTitle = id};
        tab.recipes = std::move(recipes);
        if (!tab.info.workstations.empty()) {
            tab.icon = tab.info.workstations.front();
        } else if (!tab.recipes.empty()) {
            auto const& first = index.recipe(tab.recipes.front());
            if (!first.outputs.empty() && !first.outputs.front().empty()) tab.icon = first.outputs.front().alternatives.front();
        }
        mTabs.push_back(std::move(tab));
    };

    mTabs.clear();
    if (mFocus.item.empty()) {
        if (!mFocus.category.empty()) {
            auto recipes = index.recipesInCategory(mFocus.category);
            if (!recipes.empty()) makeTab(mFocus.category, std::move(recipes));
        }
    } else {
        for (auto& group : index.lookup(mFocus.item, mFocus.mode)) makeTab(group.category, std::move(group.recipes));
    }
    mGeneration = manager.generation();
    mCellValid  = false;
    mChecks.clear();
    mInitial = false;
    if (mTabs.empty()) return false;

    int  count = static_cast<int>(mTabs.size());
    int  sel   = -1;
    if (!keep.empty()) {
        for (int i = 0; i < count; ++i) {
            if (mTabs[static_cast<std::size_t>(i)].info.id == keep) sel = i;
        }
    }
    bool found = sel >= 0;
    if (!found) sel = std::clamp(keepSel, 0, count - 1);
    mSel  = sel;
    mPage = (found || initial) ? std::max(0, keepPage) : 0; // clamped once the page size is known
    return true;
}

void RecipeLayer::selectTab(int index) {
    if (mTabs.empty()) return;
    int count = static_cast<int>(mTabs.size());
    index     = ((index % count) + count) % count;
    if (index == mSel) return;
    mSel       = index;
    mPage      = 0;
    mCellValid = false;
    mChecks.clear();
    persist();
}

int RecipeLayer::pageCount() const {
    if (mTabs.empty()) return 1;
    core::Pagination pages{static_cast<int>(mTabs[static_cast<std::size_t>(mSel)].recipes.size()), mPerPage, mPage};
    return pages.pageCount();
}

void RecipeLayer::stepPage(int delta) {
    if (mTabs.empty()) return;
    core::Pagination pages{static_cast<int>(mTabs[static_cast<std::size_t>(mSel)].recipes.size()), mPerPage, mPage};
    pages.step(delta);
    if (pages.page == mPage) return;
    mPage = pages.page;
    mChecks.clear();
    persist();
}

void RecipeLayer::persist() {
    if (mTabs.empty()) return;
    auto* current = app::Controller::get().history().current();
    if (!current || current->item != mFocus.item || current->mode != mFocus.mode) return;
    current->category    = mTabs[static_cast<std::size_t>(mSel)].info.id;
    current->categoryTab = mSel;
    current->page        = mPage;
    mFocus               = *current;
}

void RecipeLayer::measureCell() {
    if (mCellValid || mTabs.empty()) return;
    auto& index = game::RecipeManager::get().index();
    auto& tab   = mTabs[static_cast<std::size_t>(mSel)];
    Size  cell{18, 18};
    for (auto i : tab.recipes) {
        auto const& view = index.recipe(i);
        Size        s    = measure(tab.info.layout, view);
        cell.w           = std::max(cell.w, s.w);
        cell.h           = std::max(cell.h, s.h + noteHeight(view));
    }
    mCell      = cell;
    mCellValid = true;
}

actions::TransferCheck const& RecipeLayer::transferCheck(std::size_t recipe, core::RecipeView const& view, double time) {
    if (time < mChecksTime || time - mChecksTime > 0.5) {
        mChecks.clear();
        mChecksTime = time;
    }
    auto it = mChecks.find(recipe);
    if (it == mChecks.end()) it = mChecks.emplace(recipe, actions::Actions::get().checkTransfer(view)).first;
    return it->second;
}

// --- Rendering -----------------------------------------------------------------------------

bool RecipeLayer::visible(FrameInfo const&) const { return mRendering; }

void RecipeLayer::layoutFrame(Painter&, FrameInfo const& frame) {
    auto const& S   = frame.screen;
    auto const& tab = mTabs[static_cast<std::size_t>(mSel)];

    float maxW   = std::max(MIN_W, S.w - 2 * (MARGIN + STRIP));
    float wanted = std::max(MIN_W, mCell.w + PLUS + 6 + 2 * PAD + 8);
    float w      = std::floor(std::min(wanted, maxW));
    float top    = MARGIN + TAB_H - 3;
    mPanel       = {std::floor((S.w - w) / 2), top, w, std::max(80.f, S.h - top - MARGIN)};

    // Row 1: [<] category title [>] [x]
    float row1 = mPanel.y + PAD;
    mClose     = {mPanel.right() - PAD - BTN, row1 + (ROW_H - BTN) / 2, BTN, BTN};
    mTitle     = {mPanel.x + PAD, row1, mClose.x - 2 - (mPanel.x + PAD), ROW_H};
    bool multiCat = mTabs.size() > 1;
    mCatPrev = multiCat ? core::Rect{mTitle.x, mTitle.y + (ROW_H - BTN) / 2, BTN, BTN} : core::Rect{};
    mCatNext = multiCat ? core::Rect{mTitle.right() - BTN, mTitle.y + (ROW_H - BTN) / 2, BTN, BTN} : core::Rect{};

    // Recipe area and page size.
    float row2 = row1 + ROW_H + 3;
    mArea      = {mPanel.x + PAD, row2 + ROW_H + 4, mPanel.w - 2 * PAD, 0};
    mArea.h    = std::max(18.f, mPanel.bottom() - PAD - mArea.y);
    mPerPage   = std::max(1, static_cast<int>(std::floor((mArea.h + 2) / (mCell.h + 2))));
    core::Pagination pages{static_cast<int>(tab.recipes.size()), mPerPage, mPage};
    pages.clamp();
    if (pages.page != mPage) {
        mPage = pages.page;
        persist();
    }

    // Row 2: [<] page/count [>]
    bool multiPage = pages.pageCount() > 1;
    mPagePrev = multiPage ? core::Rect{mPanel.x + PAD, row2 + (ROW_H - BTN) / 2, BTN, BTN} : core::Rect{};
    mPageNext = multiPage ? core::Rect{mPanel.right() - PAD - BTN, row2 + (ROW_H - BTN) / 2, BTN, BTN} : core::Rect{};

    // Tabs: the "page" of tabs containing the selected one.
    float avail = mPanel.w - 4;
    int   fit   = std::max(1, static_cast<int>(std::floor(avail / (TAB_W + 1))));
    mFirstTab   = (mSel / fit) * fit;
    mTabsShown  = std::min(fit, static_cast<int>(mTabs.size()) - mFirstTab);
    mTabStrip   = {mPanel.x, MARGIN, static_cast<float>(mTabsShown) * (TAB_W + 1) + 2, TAB_H};
}

void RecipeLayer::drawTabs(Painter& p, FrameInfo const& frame) {
    mTabHits.clear();
    for (int k = 0; k < mTabsShown; ++k) {
        int        i = mFirstTab + k;
        auto&      t = mTabs[static_cast<std::size_t>(i)];
        float      x = mPanel.x + 2 + static_cast<float>(k) * (TAB_W + 1);
        bool       selected = i == mSel;
        core::Rect r        = selected ? core::Rect{x, MARGIN, TAB_W, TAB_H + 1} : core::Rect{x, MARGIN + 2, TAB_W, TAB_H - 2};
        mTabHits.push_back({{x, MARGIN, TAB_W, TAB_H - 3}, i});
        if (selected) continue; // drawn after the panel so it merges with it
        bool hover = r.contains(frame.mouseX, frame.mouseY);
        p.fillRect(r, palette::PANEL_BORDER);
        p.fillRect({r.x + 1, r.y + 1, r.w - 2, r.h - 1}, hover ? Color::rgb(0xB8B8B8) : TAB_BG);
        if (!t.icon.empty()) p.item(t.icon, r.x + 3, r.y + 3);
    }
}

void RecipeLayer::drawHeader(Painter& p, FrameInfo const& frame) {
    float mx = frame.mouseX, my = frame.mouseY;
    auto& tab = mTabs[static_cast<std::size_t>(mSel)];

    // Selected tab, merged into the panel's top edge.
    if (mSel >= mFirstTab && mSel < mFirstTab + mTabsShown) {
        float      x = mPanel.x + 2 + static_cast<float>(mSel - mFirstTab) * (TAB_W + 1);
        core::Rect r{x, MARGIN, TAB_W, TAB_H + 1};
        p.fillRect({r.x, r.y, r.w, r.h - 2}, palette::PANEL_BORDER);
        p.fillRect({r.x + 1, r.y + 1, r.w - 2, r.h - 1}, palette::PANEL_LIGHT);
        p.fillRect({r.x + 2, r.y + 2, r.w - 4, r.h}, palette::PANEL_BG);
        if (!tab.icon.empty()) p.item(tab.icon, r.x + 3, r.y + 3);
    }

    // Row 1: category title.
    p.fillRect(mTitle, ROW_BG);
    float textL = mTitle.x + (mCatPrev.empty() ? 2 : BTN + 2);
    float textR = mTitle.right() - (mCatNext.empty() ? 2 : BTN + 2);
    p.text(categoryTitle(tab.info), (textL + textR) / 2, mTitle.y + 2, palette::TEXT_LIGHT, 1, TextAlign::Center);
    if (!mCatPrev.empty()) {
        button(p, mCatPrev, hit(mCatPrev, mx, my));
        drawArrowLeftSmall(p, mCatPrev.x + 1, mCatPrev.y + 1, hit(mCatPrev, mx, my));
        button(p, mCatNext, hit(mCatNext, mx, my));
        drawArrowRightSmall(p, mCatNext.x + 1, mCatNext.y + 1, hit(mCatNext, mx, my));
    }
    button(p, mClose, hit(mClose, mx, my));
    closeGlyph(p, mClose, hit(mClose, mx, my));

    // Row 2: page n/m.
    core::Rect row2{mPanel.x + PAD, mTitle.bottom() + 3, mPanel.w - 2 * PAD, ROW_H};
    p.fillRect(row2, ROW_BG);
    std::string pageText = std::to_string(mPage + 1) + "/" + std::to_string(pageCount());
    p.text(pageText, row2.x + row2.w / 2, row2.y + 2, palette::TEXT_LIGHT, 1, TextAlign::Center);
    if (!mPagePrev.empty()) {
        button(p, mPagePrev, hit(mPagePrev, mx, my));
        drawArrowLeftSmall(p, mPagePrev.x + 1, mPagePrev.y + 1, hit(mPagePrev, mx, my));
        button(p, mPageNext, hit(mPageNext, mx, my));
        drawArrowRightSmall(p, mPageNext.x + 1, mPageNext.y + 1, hit(mPageNext, mx, my));
    }
}

DrawContext RecipeLayer::makeContext(Painter& p, FrameInfo const& frame) {
    return DrawContext{
        .painter = p,
        .time    = frame.time,
        .mouseX  = frame.mouseX,
        .mouseY  = frame.mouseY,
        .focus   = mFocus.item.empty() ? nullptr : &mFocus.item,
        .mode    = mFocus.mode,
        .slots   = &mSlots,
        .hints   = &mHints,
    };
}

void RecipeLayer::drawRecipes(Painter& p, FrameInfo const& frame) {
    auto& index = game::RecipeManager::get().index();
    auto& tab   = mTabs[static_cast<std::size_t>(mSel)];
    mPlus.clear();

    float total = static_cast<float>(mPerPage) * mCell.h;
    float gap   = std::max(0.f, (mArea.h - total) / static_cast<float>(mPerPage + 1));
    float cellX = mArea.x + std::floor((mArea.w - (mCell.w + PLUS + 6)) / 2);

    auto ctx = makeContext(p, frame);
    for (int i = 0; i < mPerPage; ++i) {
        auto slot = static_cast<std::size_t>(mPage * mPerPage + i);
        if (slot >= tab.recipes.size()) break;
        std::size_t recipeIndex = tab.recipes[slot];
        auto const& view        = index.recipe(recipeIndex);
        float       y           = std::floor(mArea.y + gap + static_cast<float>(i) * (mCell.h + gap));

        if (i > 0 && gap >= 4) {
            p.fillRect({mArea.x + 8, std::floor(y - gap / 2), mArea.w - 16, 1}, SEPARATOR);
        }

        // ---- Recipe transfer ("+") -------------------------------------------------------
        // actions::Actions decides whether the recipe can be moved into the open crafting
        // grid; the button only appears when it can, and missing inputs are tinted red.
        auto const& check = transferCheck(recipeIndex, view, frame.time);
        ctx.missingInputs = check.possible && !check.missingInputs.empty() ? &check.missingInputs : nullptr;

        draw(tab.info.layout, view, cellX, y, mCell, ctx);

        if (check.possible) {
            core::Rect r{cellX + mCell.w + 6, y + mCell.h - noteHeight(view) - PLUS, PLUS, PLUS};
            bool       hover = r.contains(frame.mouseX, frame.mouseY);
            button(p, r, hover, check.canCraft() ? PLUS_OK : PLUS_MISS);
            p.text("+", r.x + r.w / 2, r.y + 2, palette::TEXT_LIGHT, 1, TextAlign::Center);
            mPlus.push_back({r, recipeIndex, check.canCraft()});
        }
    }
}

void RecipeLayer::drawWorkstations(Painter& p, FrameInfo const& frame) {
    auto& tab = mTabs[static_cast<std::size_t>(mSel)];
    if (tab.info.workstations.empty()) return;
    float maxH  = mPanel.h - 12;
    auto  count = std::min(tab.info.workstations.size(), static_cast<std::size_t>(std::max(1.f, std::floor((maxH - 8) / 18))));
    core::Rect strip{mPanel.x - STRIP, mPanel.y + 6, STRIP + 4, static_cast<float>(count) * 18 + 8};
    p.panel(strip);
    auto ctx = makeContext(p, frame);
    for (std::size_t i = 0; i < count; ++i) {
        core::IngredientView ws{.alternatives = {tab.info.workstations[i]}};
        ingredientSlot(ctx, ws, strip.x + 4, strip.y + 4 + static_cast<float>(i) * 18, SlotRole::Catalyst, {});
    }
}

void RecipeLayer::drawTooltips(Painter& p, FrameInfo const& frame) {
    float mx = frame.mouseX, my = frame.mouseY;
    for (auto const& s : mSlots) {
        if (!s.rect.contains(mx, my)) continue;
        std::vector<TooltipLine> extra;
        if (s.alternatives > 1) {
            extra.push_back({substitute(app::tr("bei.recipe.alternatives", "Accepts any of {} items"), std::to_string(s.alternatives)), 0xAAAAAA});
        }
        if (s.role == SlotRole::Catalyst && s.recipeId.empty()) {
            extra.push_back({app::tr("bei.recipe.workstation", "Workstation"), 0x8080FF});
        }
        if (s.role == SlotRole::Output && !s.recipeId.empty()) {
            extra.push_back({substitute(app::tr("bei.recipe.id", "Recipe: {}"), s.recipeId), 0x808080});
        }
        drawItemTooltip(p, s.item, mx, my, frame, extra);
        return;
    }
    for (auto const& h : mHints) {
        if (h.rect.contains(mx, my)) return drawTooltip(p, {{h.text}}, mx, my, frame);
    }
    for (auto const& plus : mPlus) {
        if (!plus.rect.contains(mx, my)) continue;
        std::vector<TooltipLine> lines;
        if (plus.canCraft) lines.push_back({app::tr("bei.recipe.transfer", "Move ingredients")});
        else lines.push_back({app::tr("bei.recipe.transfer.missing", "Missing ingredients"), 0xFF5555});
        lines.push_back({app::tr("bei.recipe.transfer.max", "Shift-click: move as many as possible"), 0xAAAAAA});
        return drawTooltip(p, lines, mx, my, frame);
    }
    for (auto const& t : mTabHits) {
        if (!t.rect.contains(mx, my)) continue;
        auto& tab = mTabs[static_cast<std::size_t>(t.index)];
        return drawTooltip(
            p,
            {{categoryTitle(tab.info)},
             {substitute(app::tr("bei.recipe.count", "Recipes: {}"), std::to_string(tab.recipes.size())), 0xAAAAAA}},
            mx,
            my,
            frame
        );
    }
    auto simple = [&](core::Rect const& r, char const* key, char const* fallback) {
        if (!hit(r, mx, my)) return false;
        drawTooltip(p, {{app::tr(key, fallback)}}, mx, my, frame);
        return true;
    };
    if (simple(mClose, "bei.recipe.close", "Close (Esc)")) return;
    if (simple(mCatPrev, "bei.recipe.category.prev", "Previous category")) return;
    if (simple(mCatNext, "bei.recipe.category.next", "Next category")) return;
    if (simple(mPagePrev, "bei.recipe.page.prev", "Previous page")) return;
    if (simple(mPageNext, "bei.recipe.page.next", "Next page")) return;
}

void RecipeLayer::render(Painter& p, FrameInfo const& frame) {
    mSlots.clear();
    mHints.clear();
    mPlus.clear();
    mTabHits.clear();
    bool ok     = refresh();
    mHasContent = ok;
    if (!ok) {
        // Nothing to show any more (e.g. the recipe index was rebuilt without these recipes).
        app::Controller::get().closeRecipes();
        return;
    }
    measureCell();
    layoutFrame(p, frame);

    drawTabs(p, frame);
    drawWorkstations(p, frame);
    p.panel(mPanel);
    drawHeader(p, frame);
    drawRecipes(p, frame);

    p.flush();
    drawTooltips(p, frame);
}

// --- Input ---------------------------------------------------------------------------------

// Read on the window thread by InputRouter, hence the atomic instead of mTabs.
bool RecipeLayer::contains(float, float) const { return mHasContent.load(); }

std::optional<core::ItemKey> RecipeLayer::itemAt(float x, float y) const {
    for (auto const& s : mSlots) {
        if (s.rect.contains(x, y)) return s.item;
    }
    return std::nullopt;
}

bool RecipeLayer::onMouseButton(MouseButton button, bool down, float x, float y) {
    if (mTabs.empty()) return false;
    if (!down) return true; // the screen swallows all input
    auto& controller = app::Controller::get();
    bool  left       = button == MouseButton::Left;
    bool  right      = button == MouseButton::Right;

    for (auto const& t : mTabHits) {
        if (t.rect.contains(x, y) && (left || right)) {
            selectTab(t.index);
            return true;
        }
    }
    if (left) {
        if (hit(mClose, x, y)) return controller.closeRecipes(), true;
        if (hit(mCatPrev, x, y)) return selectTab(mSel - 1), true;
        if (hit(mCatNext, x, y)) return selectTab(mSel + 1), true;
        if (hit(mPagePrev, x, y)) return stepPage(-1), true;
        if (hit(mPageNext, x, y)) return stepPage(1), true;
        for (auto const& plus : mPlus) {
            if (!plus.rect.contains(x, y)) continue;
            auto& index = game::RecipeManager::get().index();
            if (plus.recipe < index.size() && actions::Actions::get().transfer(index.recipe(plus.recipe), shiftHeld())) {
                controller.closeRecipes(); // back to the crafting screen, like JEI
            }
            mChecks.clear();
            return true;
        }
    }
    if (left || right) {
        if (auto item = itemAt(x, y)) {
            core::ItemKey key = *item; // the call below re-targets this layer
            if (left) controller.showRecipes(key);
            else controller.showUses(key);
        }
    }
    return true;
}

bool RecipeLayer::onScroll(float x, float y, float delta) {
    if (mTabs.empty()) return false;
    if (delta == 0) return true;
    int step = delta > 0 ? -1 : 1;
    if (mTabStrip.contains(x, y)) selectTab(mSel + step);
    else stepPage(step);
    return true;
}

bool RecipeLayer::isScreenKey(int vk) noexcept {
    switch (vk) {
    case VK_BACK_:
    case VK_ESCAPE_:
    case VK_PRIOR_:
    case VK_NEXT_:
    case VK_LEFT_:
    case VK_UP_:
    case VK_RIGHT_:
    case VK_DOWN_:
        return true;
    default:
        return false;
    }
}

bool RecipeLayer::handleKey(int vk) {
    auto& controller = app::Controller::get();
    switch (vk) {
    case VK_ESCAPE_: controller.closeRecipes(); return true;
    case VK_BACK_: controller.back(); return true;
    case VK_LEFT_:
    case VK_PRIOR_: stepPage(-1); return true;
    case VK_RIGHT_:
    case VK_NEXT_: stepPage(1); return true;
    case VK_UP_: selectTab(mSel - 1); return true;
    case VK_DOWN_: selectTab(mSel + 1); return true;
    default: return false;
    }
}

BEI_REGISTER_UI_LAYER(RecipeLayer::get());

} // namespace bei::ui::recipe
