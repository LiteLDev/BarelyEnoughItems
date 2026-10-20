#include "ui/widgets/Tooltip.h"

#include "app/Controller.h"
#include "app/I18n.h"
#include "game/items/ItemCatalog.h"
#include "ui/gfx/Painter.h"
#include "ui/widgets/Utf8.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace bei::ui {

namespace {
// Vanilla (Java) tooltip colours: near-black purple body, purple gradient border.
constexpr Color BG            = Color::rgb(0x100010, 0xF0 / 255.f);
constexpr Color BORDER_TOP    = Color::rgb(0x5000FF, 0x50 / 255.f);
constexpr Color BORDER_BOTTOM = Color::rgb(0x28007F, 0x50 / 255.f);

constexpr float LINE_H    = 10; // 9 font + 1 spacing
constexpr float TITLE_GAP = 2;  // extra space under the first line
constexpr float MARGIN    = 4;  // minimum distance to the screen edge (outside the border)

constexpr std::uint32_t ID_RGB   = 0x808080;
constexpr std::uint32_t HINT_RGB = 0xAAAAAA;
constexpr std::uint32_t MOD_RGB  = 0x5555FF; // JEI shows the mod name in blue (italic in Java)

Color lerp(Color a, Color b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

/// Formatting codes ("§x") active at the end of `s`, to carry them over to a wrapped line.
std::string activeFormat(std::string_view s, std::string fmt) {
    static constexpr std::string_view SECTION = "\xC2\xA7"; // '§'
    for (std::size_t i = s.find(SECTION); i != std::string_view::npos; i = s.find(SECTION, i + 1)) {
        if (i + 2 >= s.size()) break;
        char code = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i + 2])));
        if (std::isxdigit(static_cast<unsigned char>(code)) || code == 'r') fmt.clear(); // colour / reset
        if (code != 'r') fmt += std::string{SECTION} + code;
    }
    return fmt;
}

/// Greedy word wrap at `maxW`; CJK text may break between any two characters.
std::vector<std::string> wrap(Painter& p, std::string const& text, float maxW) {
    if (text.empty() || p.textWidth(text) <= maxW) return {text};
    std::vector<std::string> out;
    std::string              fmt;
    std::size_t              start = 0;
    while (start < text.size()) {
        std::size_t pos = start, lastBreak = std::string::npos;
        while (pos < text.size()) {
            char32_t    cp  = utf8::decode(text, pos);
            std::size_t nxt = utf8::next(text, pos);
            if (cp == U'§' && nxt < text.size()) nxt = utf8::next(text, nxt); // keep "§x" together
            if (pos > start && p.textWidth(fmt + text.substr(start, nxt - start)) > maxW) break;
            if (utf8::isSpace(cp) || (utf8::isWide(cp) && pos > start)) lastBreak = pos;
            pos = nxt;
            if (utf8::isWide(cp)) lastBreak = pos;
        }
        if (pos >= text.size()) {
            out.push_back(fmt + text.substr(start));
            break;
        }
        std::size_t cut = lastBreak != std::string::npos && lastBreak > start ? lastBreak : pos;
        out.push_back(fmt + text.substr(start, cut - start));
        fmt   = activeFormat(std::string_view{text}.substr(start, cut - start), fmt);
        start = cut;
        while (start < text.size() && utf8::isSpace(utf8::decode(text, start))) start = utf8::next(text, start);
    }
    return out;
}

/// "minecraft" -> "Minecraft", "my_mod" -> "My Mod" (used when no translation exists).
std::string prettyNamespace(std::string_view ns) {
    std::string out;
    bool        upper = true;
    for (char c : ns) {
        if (c == '_' || c == '-' || c == '.') {
            out   += ' ';
            upper  = true;
            continue;
        }
        out   += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        upper  = false;
    }
    return out;
}
} // namespace

void drawTooltip(Painter& p, std::vector<TooltipLine> const& lines, float x, float y, FrameInfo const& frame) {
    if (lines.empty()) return;
    p.flush(); // everything drawn so far stays below the tooltip

    core::Rect const screen = frame.screen.empty() ? p.screen() : frame.screen;
    float const      maxW   = std::clamp(std::floor(screen.w * 0.4f), 120.f, 260.f);

    struct Row {
        std::string   text;
        std::uint32_t rgb;
    };
    std::vector<Row> rows;
    std::size_t      titleRows = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        for (auto& part : wrap(p, lines[i].text, maxW)) rows.push_back({std::move(part), lines[i].rgb});
        if (i == 0) titleRows = rows.size();
    }

    // Keep the tooltip on screen vertically: drop rows that do not fit and say so.
    float gap     = rows.size() > titleRows ? TITLE_GAP : 0;
    auto  maxRows = static_cast<std::size_t>(std::max(1.f, (screen.h - 2 * MARGIN - 8 - gap + 1) / LINE_H));
    if (rows.size() > maxRows) {
        std::size_t hidden = rows.size() - (maxRows - 1);
        rows.resize(maxRows - 1);
        auto more = app::tr("bei.widgets.tooltip.more", "... ({} more)");
        if (auto at = more.find("{}"); at != std::string::npos) more.replace(at, 2, std::to_string(hidden));
        rows.push_back({std::move(more), ID_RGB});
        if (rows.size() <= titleRows) gap = 0;
    }

    float w = 0;
    for (auto const& r : rows) w = std::max(w, p.textWidth(r.text));
    w       = std::ceil(w);
    float h = static_cast<float>(rows.size()) * LINE_H - 1 + gap;

    // Body origin (text area); the frame extends 4 units around it.
    float bx = x + 12, by = y - 12;
    if (bx + w + 4 > screen.right() - MARGIN) bx = x - 16 - w;
    bx = std::clamp(bx, screen.x + MARGIN + 4, std::max(screen.x + MARGIN + 4, screen.right() - MARGIN - 4 - w));
    by = std::clamp(by, screen.y + MARGIN + 4, std::max(screen.y + MARGIN + 4, screen.bottom() - MARGIN - 4 - h));

    constexpr auto FG = Painter::Layer::Foreground;
    // Background with the vanilla "rounded" corners (the 1-unit strips stop short of the corners).
    p.fillRect({bx - 3, by - 4, w + 6, 1}, BG, FG);
    p.fillRect({bx - 3, by + h + 3, w + 6, 1}, BG, FG);
    p.fillRect({bx - 3, by - 3, w + 6, h + 6}, BG, FG);
    p.fillRect({bx - 4, by - 3, 1, h + 6}, BG, FG);
    p.fillRect({bx + w + 3, by - 3, 1, h + 6}, BG, FG);
    // Border: top and bottom lines, vertical gradient on the sides.
    p.fillRect({bx - 3, by - 3, w + 6, 1}, BORDER_TOP, FG);
    p.fillRect({bx - 3, by + h + 2, w + 6, 1}, BORDER_BOTTOM, FG);
    float sideTop = by - 2, sideH = h + 4;
    int   steps = std::max(1, static_cast<int>(sideH / 2));
    for (int i = 0; i < steps; ++i) {
        float y0 = sideTop + sideH * static_cast<float>(i) / static_cast<float>(steps);
        float y1 = sideTop + sideH * static_cast<float>(i + 1) / static_cast<float>(steps);
        Color c  = lerp(BORDER_TOP, BORDER_BOTTOM, (static_cast<float>(i) + 0.5f) / static_cast<float>(steps));
        p.fillRect({bx - 3, y0, 1, y1 - y0}, c, FG);
        p.fillRect({bx + w + 2, y0, 1, y1 - y0}, c, FG);
    }

    float ly = by;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        p.text(rows[i].text, bx, ly, Color::rgb(rows[i].rgb));
        ly += LINE_H;
        if (i + 1 == titleRows) ly += gap;
    }
    p.flush();
}

void drawItemTooltip(
    Painter&                        p,
    core::ItemKey const&            item,
    float                           x,
    float                           y,
    FrameInfo const&                frame,
    std::vector<TooltipLine> const& extra
) {
    std::string name = game::ItemCatalog::get().displayName(item);
    if (name.empty()) name = std::string{item.path()};

    std::vector<TooltipLine> lines{
        {std::move(name), 0xFFFFFF}
    };
    lines.insert(lines.end(), extra.begin(), extra.end());
    auto const& cfg = app::Controller::get().config();
    if (cfg.showItemIds) lines.push_back({item.toString(), ID_RGB}); // JEI: advanced tooltips
    lines.push_back({app::tr("bei.tooltip.hint", "R: Recipes   U: Uses   A: Bookmark"), HINT_RGB});
    if (cfg.showModName) {
        auto ns = std::string{item.nameSpace()};
        lines.push_back({app::tr("bei.widgets.mod." + ns, prettyNamespace(ns)), MOD_RGB});
    }
    drawTooltip(p, lines, x, y, frame);
}

} // namespace bei::ui
