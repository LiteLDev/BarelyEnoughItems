// In-game settings (JEI's wrench button): a button right of the item panel search bar and a
// compact popup with a toggle for every user-facing core::Config field.
//
// - Left click on the button opens/closes the popup; Ctrl+click toggles cheat mode directly.
// - The button is tinted red while cheat mode is on; its tooltip says so.
// - Cheat mode can only be turned on when actions::Actions::canCheat(); otherwise the toggle is
//   disabled and the tooltips explain why.
// - Every change is applied at once and persisted with app::Controller::save().
// - The popup closes on a click outside it, on a second click on the button and when the screen
//   is reopened.
#include "actions/Actions.h"
#include "app/Controller.h"
#include "app/I18n.h"
#include "core/state/Layout.h"
#include "mod/Log.h"
#include "ui/UiRoot.h"
#include "ui/gfx/Painter.h"
#include "ui/overlay/ItemListPanel.h"
#include "ui/overlay/PagedItemGrid.h"
#include "ui/widgets/Button.h"
#include "ui/widgets/Tooltip.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <vector>

// Avoid <windows.h> here (macro clashes with game headers); only this one call is needed.
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
#pragma comment(lib, "user32")

namespace bei::ui {

namespace {

constexpr int   VK_CONTROL_ = 0x11;
constexpr float PAD         = 5;  ///< Inner padding of the popup.
constexpr float ROW_H       = 12; ///< Height of one setting row.
constexpr float BOX         = 9;  ///< Checkbox size.
constexpr float SMALL_BTN   = 11; ///< -/+ buttons of the columns row.
constexpr float GAP         = 2;  ///< Popup distance from the settings button.
constexpr double CTRL_GRACE = 0.3; ///< Seconds a Ctrl seen held still counts for a click.

constexpr std::uint32_t CHEAT_TINT = 0xB03030;
constexpr std::uint32_t RED_TEXT   = 0xFF5555;
constexpr std::uint32_t GREY_TEXT  = 0xAAAAAA;

bool ctrlDown() { return (GetAsyncKeyState(VK_CONTROL_) & 0x8000) != 0; }

std::string format1(std::string text, std::string const& arg) {
    if (auto pos = text.find("{}"); pos != std::string::npos) text.replace(pos, 2, arg);
    return text;
}

/// A boolean Config field shown as a checkbox row.
struct ToggleDef {
    char const*        id;       ///< Log / e2e name ("cheatMode", ...), the JSON key.
    char const*        key;      ///< Translation key.
    char const*        fallback; ///< English text.
    bool core::Config::* field;
};

constexpr std::array<ToggleDef, 8> TOGGLES{
    {
     {"cheatMode", "bei.settings.cheat_mode", "Cheat mode", &core::Config::cheatMode},
     {"bookmarksEnabled", "bei.settings.bookmarks", "Bookmark panel", &core::Config::bookmarksEnabled},
     {"bookmarksAddToFront",
         "bei.settings.bookmarks_front",
         "Add new bookmarks to the front",
         &core::Config::bookmarksAddToFront},
     {"centerSearchBar",
         "bei.settings.center_search",
         "Search bar under the container",
         &core::Config::centerSearchBar},
     {"showItemIds", "bei.settings.item_ids", "Show item ids in tooltips", &core::Config::showItemIds},
     {"showModName", "bei.settings.mod_name", "Show mod name in tooltips", &core::Config::showModName},
     {"rememberPage", "bei.settings.remember_page", "Remember the item list page", &core::Config::rememberPage},
     {"showOnAllContainers",
         "bei.settings.all_containers",
         "Show on all container screens",
         &core::Config::showOnAllContainers},
     }
};
constexpr std::size_t CHEAT_ROW = 0;

class SettingsLayer final : public IUiLayer {
public:
    [[nodiscard]] char const* name() const override { return "settings"; }
    /// Above the overlay panels and their item tooltip (150), below the vanilla tooltips.
    [[nodiscard]] int  zOrder() const override { return 200; }
    [[nodiscard]] bool visible(FrameInfo const& frame) const override { return overlayAllowed(frame); }

    void render(Painter& p, FrameInfo const& frame) override {
        if (reopened(frame, mLastTime)) {
            mOpen = false;
            loadStrings();
        }
        mButton.rect = ItemListPanel::get().configButtonRect();
        if (mButton.rect.empty()) mOpen = false;
        if (!mOpen) setOverlayPopup({});
        if (mButton.rect.empty()) return;
        logButtonIfMoved(frame);
        if (ctrlDown()) mCtrlSeenAt = frame.time;

        auto& cfg       = app::Controller::get().config();
        bool  canCheat  = actions::Actions::get().canCheat();
        mButton.glyph   = Glyph::Wrench;
        mButton.toggled = mOpen;
        mButton.tint    = cfg.cheatMode ? std::optional<std::uint32_t>{CHEAT_TINT} : std::nullopt;
        mButton.render(p, frame.mouseX, frame.mouseY);

        if (mOpen) {
            renderPopup(p, frame, canCheat);
            setOverlayPopup(mPopup);
        }

        // Tooltips last (drawTooltip flushes, so they stay on top). The button's own tooltip is
        // hidden while the popup is open: it would cover the popup's bottom rows.
        if (!mOpen && mButton.rect.contains(frame.mouseX, frame.mouseY)) {
            std::vector<TooltipLine> lines{
                {mTitle, 0xFFFFFF}
            };
            if (cfg.cheatMode) lines.push_back({mCheatOn, RED_TEXT});
            lines.push_back({mOpenHint, GREY_TEXT});
            lines.push_back({mCheatHint, GREY_TEXT});
            if (!canCheat) lines.push_back({mCheatUnavailable, RED_TEXT});
            drawTooltip(p, lines, frame.mouseX, frame.mouseY, frame);
        } else if (mOpen && !canCheat && mRows[CHEAT_ROW].contains(frame.mouseX, frame.mouseY)) {
            drawTooltip(p, {{mCheatUnavailable, RED_TEXT}}, frame.mouseX, frame.mouseY, frame);
        }
    }

    bool onMouseButton(MouseButton button, bool down, float x, float y) override {
        if (mButton.rect.contains(x, y)) {
            if (button == MouseButton::Left && down) {
                // The click is handled a little after it happened: a Ctrl seen held just before
                // counts too (TextBox does the same for its shortcuts).
                bool ctrl = ctrlDown() || UiRoot::get().lastFrame().time - mCtrlSeenAt < CTRL_GRACE;
                if (ctrl) {
                    toggleCheat("Ctrl+click");
                } else {
                    setOpen(!mOpen);
                }
            }
            return true;
        }
        if (!mOpen) return false;
        if (!mPopup.contains(x, y)) {
            if (down) setOpen(false); // click elsewhere closes it (and goes on to what is below)
            return false;
        }
        if (button == MouseButton::Left && down) click(x, y);
        return true;
    }

    bool onScroll(float x, float y, float) override { return contains(x, y); }

    [[nodiscard]] bool contains(float x, float y) const override {
        return mButton.rect.contains(x, y) || (mOpen && mPopup.contains(x, y));
    }

private:
    void setOpen(bool open) {
        mOpen = open;
        // Item tooltips under the popup are suppressed from now on (the tooltip layer draws before
        // this one; mPopup is where the popup was last drawn, refreshed by the next render).
        setOverlayPopup(open ? mPopup : core::Rect{});
        log().info("Settings popup {}", open ? "opened" : "closed");
    }

    void loadStrings() {
        mTitle            = app::tr("bei.settings.title", "BEI Settings");
        mOpenHint         = app::tr("bei.settings.open_hint", "Click: settings");
        mCheatHint        = app::tr("bei.settings.cheat_hint", "Ctrl+Click: toggle cheat mode");
        mCheatOn          = app::tr("bei.settings.cheat_on", "Cheat mode is ON (click items to get them)");
        mCheatUnavailable = app::tr(
            "bei.settings.cheat_unavailable",
            "Cheat mode needs cheats enabled and operator permission"
        );
        mColumns     = app::tr("bei.settings.columns", "Max columns: {}");
        mColumnsAuto = app::tr("bei.settings.columns_auto", "Auto ({})");
        for (std::size_t i = 0; i < TOGGLES.size(); ++i) mLabels[i] = app::tr(TOGGLES[i].key, TOGGLES[i].fallback);
        mLoggedPopup = {};
    }

    void logButtonIfMoved(FrameInfo const& frame) {
        auto const& r = mButton.rect;
        if (r == mLoggedButton || frame.screen.empty()) return;
        mLoggedButton = r;
        log().info(
            "Settings button at {:.3f},{:.3f}",
            (r.x + r.w / 2) / frame.screen.w,
            (r.y + r.h / 2) / frame.screen.h
        );
    }

    void renderPopup(Painter& p, FrameInfo const& frame, bool canCheat) {
        auto& cfg    = app::Controller::get().config();
        auto  layout = overlayLayout(frame);
        // Columns the item panel shows with maxColumns = 0 (auto).
        mFitCols = layout.itemPanelUsable ? core::fitGrid(layout.itemGrid, PagedItemGrid::CELL, 0).cols : 0;
        std::string columnsValue =
            cfg.maxColumns == 0 ? format1(mColumnsAuto, std::to_string(mFitCols)) : std::to_string(cfg.maxColumns);
        mColumnsLabel = format1(mColumns, columnsValue);

        // Size: widest label + checkbox, or the columns label + its two buttons.
        float textW = p.textWidth(mTitle);
        for (auto const& label : mLabels) textW = std::max(textW, BOX + 4 + p.textWidth(label));
        textW   = std::max(textW, p.textWidth(mColumnsLabel) + 4 + 2 * SMALL_BTN + 2);
        float w = std::ceil(textW + 2 * PAD);
        float h = PAD + ROW_H + 2 + static_cast<float>(TOGGLES.size() + 1) * ROW_H + PAD;

        // Right-aligned with the item panel area, which (unlike the button) does not move when the
        // column count changes, so -/+ stay under the cursor; with the bar under the container,
        // with the button.
        auto const& screen = frame.screen;
        float       right  = layout.searchBarCentered || !layout.itemPanelUsable ? mButton.rect.right()
                                                                                 : layout.itemGrid.right();
        float x = std::clamp(right - w, screen.x + 2, std::max(screen.x + 2, screen.right() - w - 2));
        float       y      = mButton.rect.y - GAP - h;
        if (y < screen.y + 2) y = std::min(mButton.rect.bottom() + GAP, std::max(screen.y + 2, screen.bottom() - h - 2));
        mPopup = {x, y, w, h};

        p.panel(mPopup);
        float rowY = y + PAD;
        p.text(mTitle, x + PAD, rowY + 2, palette::TEXT);
        rowY += ROW_H + 2;
        p.fillRect({x + PAD, rowY - 2, w - 2 * PAD, 1}, Color::rgb(0x8B8B8B));

        for (std::size_t i = 0; i < TOGGLES.size(); ++i) {
            core::Rect row{x + PAD - 1, rowY, w - 2 * PAD + 2, ROW_H};
            mRows[i]      = row;
            bool on       = cfg.*TOGGLES[i].field;
            bool enabled  = i != CHEAT_ROW || canCheat || on; // cheat mode can always be turned off
            bool hover    = enabled && row.contains(frame.mouseX, frame.mouseY);
            if (hover) p.fillRect(row, Color::rgb(0xFFFFFF, 0.45f));
            core::Rect box{x + PAD, rowY + (ROW_H - BOX) / 2, BOX, BOX};
            p.fillRect(box, Color::rgb(0x373737));
            p.fillRect({box.x + 1, box.y + 1, box.w - 2, box.h - 2}, Color::rgb(enabled ? 0x8B8B8B : 0x6A6A6A));
            if (on) {
                Color check = i == CHEAT_ROW ? Color::rgb(RED_TEXT) : palette::TEXT_LIGHT;
                drawGlyph(p, Glyph::Check, {box.x + 1, box.y + 1, BOX - 2, BOX - 2}, check);
            }
            Color text = !enabled ? Color::rgb(0x808080) : i == CHEAT_ROW && on ? Color::rgb(0xA01010) : palette::TEXT;
            p.text(mLabels[i], box.right() + 4, rowY + (ROW_H - p.lineHeight()) / 2 + 1, text);
            rowY += ROW_H;
        }

        // Max columns: label, then [-] [+] at the right edge.
        mColumnsRow = {x + PAD - 1, rowY, w - 2 * PAD + 2, ROW_H};
        float by    = rowY + (ROW_H - SMALL_BTN) / 2;
        mPlus.rect  = {x + w - PAD - SMALL_BTN, by, SMALL_BTN, SMALL_BTN};
        mMinus.rect = {mPlus.rect.x - 2 - SMALL_BTN, by, SMALL_BTN, SMALL_BTN};
        mMinus.glyph = Glyph::Minus;
        mPlus.glyph  = Glyph::Plus;
        mMinus.enabled = currentColumns(cfg) > PagedItemGrid::MIN_COLS;
        mPlus.enabled  = cfg.maxColumns != 0;
        p.text(mColumnsLabel, x + PAD, rowY + (ROW_H - p.lineHeight()) / 2 + 1, palette::TEXT);
        mMinus.render(p, frame.mouseX, frame.mouseY);
        mPlus.render(p, frame.mouseX, frame.mouseY);

        if (mPopup != mLoggedPopup && !screen.empty()) {
            mLoggedPopup = mPopup;
            auto at     = [&](core::Rect const& r) {
                return std::format("{:.3f},{:.3f}", (r.x + r.w / 2) / screen.w, (r.y + r.h / 2) / screen.h);
            };
            for (std::size_t i = 0; i < TOGGLES.size(); ++i) log().info("Settings row {} at {}", TOGGLES[i].id, at(mRows[i]));
            log().info("Settings row maxColumnsMinus at {}", at(mMinus.rect));
            log().info("Settings row maxColumnsPlus at {}", at(mPlus.rect));
        }
    }

    /// Columns currently shown: maxColumns, or all that fit when it is 0 (auto).
    [[nodiscard]] int currentColumns(core::Config const& cfg) const {
        return cfg.maxColumns == 0 ? mFitCols : std::min(cfg.maxColumns, std::max(mFitCols, PagedItemGrid::MIN_COLS));
    }

    void click(float x, float y) {
        auto& controller = app::Controller::get();
        auto& cfg        = controller.config();
        for (std::size_t i = 0; i < TOGGLES.size(); ++i) {
            if (!mRows[i].contains(x, y)) continue;
            if (i == CHEAT_ROW) {
                toggleCheat("settings");
                return;
            }
            bool& field = cfg.*TOGGLES[i].field;
            field       = !field;
            controller.save();
            log().info("Settings: {} = {}", TOGGLES[i].id, field);
            return;
        }
        if (mMinus.hit(x, y)) {
            cfg.maxColumns = std::max(PagedItemGrid::MIN_COLS, currentColumns(cfg) - 1);
        } else if (mPlus.hit(x, y)) {
            int next       = currentColumns(cfg) + 1;
            cfg.maxColumns = next >= mFitCols ? 0 : next; // reaching the fitting width means "auto"
        } else {
            return;
        }
        controller.save();
        log().info("Settings: maxColumns = {}", cfg.maxColumns);
    }

    void toggleCheat(char const* via) {
        auto& controller = app::Controller::get();
        auto& cfg        = controller.config();
        if (!cfg.cheatMode && !actions::Actions::get().canCheat()) {
            log().info("Settings: cheat mode not available ({})", via);
            return; // the tooltip explains why
        }
        cfg.cheatMode = !cfg.cheatMode;
        controller.save();
        log().info("Settings: cheatMode = {} ({})", cfg.cheatMode, via);
    }

    double      mLastTime   = -1;
    double      mCtrlSeenAt = -1; ///< Frame time Ctrl was last seen held.
    bool        mOpen     = false;
    Button      mButton;
    core::Rect  mPopup;
    std::array<core::Rect, TOGGLES.size()> mRows{};
    std::array<std::string, TOGGLES.size()> mLabels{};
    core::Rect  mColumnsRow;
    Button      mMinus, mPlus;
    int         mFitCols = 0;
    std::string mTitle, mOpenHint, mCheatHint, mCheatOn, mCheatUnavailable, mColumns, mColumnsAuto, mColumnsLabel;
    core::Rect  mLoggedButton;
    core::Rect  mLoggedPopup;
};

SettingsLayer gSettingsLayer;

} // namespace

BEI_REGISTER_UI_LAYER(gSettingsLayer);

} // namespace bei::ui
