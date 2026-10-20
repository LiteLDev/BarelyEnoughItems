#include "ui/gfx/Painter.h"

#include "game/items/ItemCatalog.h"

#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include <variant>

namespace bei::ui {

namespace {
struct RectCmd {
    core::Rect r;
    Color      c;
};
struct ItemCmd {
    core::ItemKey    key;
    ItemStack const* stack; // non-null when drawing a caller-owned stack (valid within the frame)
    float            x, y, scale, alpha;
};
struct TextCmd {
    std::string s;
    float       x, y;
    Color       c;
    float       scale;
    TextAlign   align;
    float       width;
};
struct TextureCmd {
    std::string path;
    core::Rect  r;
    float       alpha;
};

mce::Color toMce(Color c) { return mce::Color{c.r, c.g, c.b, c.a}; }

RectangleArea toArea(core::Rect const& r) { return RectangleArea{r.x, r.right(), r.y, r.bottom()}; }

Font& uiFont(MinecraftUIRenderContext& ctx) { return ctx.mClient.getFontHandle().getFont(); }
} // namespace

struct Painter::Cmd {
    Layer                                               layer;
    std::variant<RectCmd, ItemCmd, TextCmd, TextureCmd> v;
};

Painter::Painter(MinecraftUIRenderContext& ctx) : mCtx(ctx), mCmds(new std::vector<Cmd>) {}

Painter::~Painter() {
    flush();
    delete mCmds;
}

void Painter::fillRect(core::Rect const& r, Color c, Layer layer) {
    if (r.empty()) return;
    mCmds->push_back({layer, RectCmd{r, c}});
}

void Painter::outlineRect(core::Rect const& r, Color c, float t, Layer layer) {
    fillRect({r.x, r.y, r.w, t}, c, layer);
    fillRect({r.x, r.bottom() - t, r.w, t}, c, layer);
    fillRect({r.x, r.y + t, t, r.h - 2 * t}, c, layer);
    fillRect({r.right() - t, r.y + t, t, r.h - 2 * t}, c, layer);
}

void Painter::panel(core::Rect const& r) {
    fillRect(r, palette::PANEL_BORDER);
    fillRect({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, palette::PANEL_LIGHT);
    fillRect({r.x + 2, r.y + 2, r.w - 3, r.h - 3}, Color::rgb(0x555555));
    fillRect({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, palette::PANEL_BG);
}

void Painter::slot(float x, float y, float size) {
    fillRect({x, y, size, size}, Color::rgb(0x373737));
    fillRect({x + 1, y + 1, size - 1, size - 1}, palette::PANEL_LIGHT);
    fillRect({x + 1, y + 1, size - 2, size - 2}, palette::SLOT_BG);
}

void Painter::item(core::ItemKey const& key, float x, float y, float scale, float alpha) {
    if (key.empty()) return;
    mCmds->push_back({Layer::Items, ItemCmd{key, nullptr, x, y, scale, alpha}});
}

void Painter::itemStack(ItemStack const& stack, float x, float y, float scale, float alpha) {
    mCmds->push_back({Layer::Items, ItemCmd{{}, &stack, x, y, scale, alpha}});
}

void Painter::count(int n, float x, float y, float scale) {
    if (n <= 1) return;
    auto s = std::to_string(n);
    text(s, x, y + 16 * scale - lineHeight(scale) + 1, palette::TEXT_LIGHT, scale, TextAlign::Right, 17 * scale);
}

void Painter::text(std::string const& s, float x, float y, Color c, float scale, TextAlign align, float width) {
    if (s.empty()) return;
    mCmds->push_back({Layer::Text, TextCmd{s, x, y, c, scale, align, width}});
}

float Painter::textWidth(std::string const& s, float scale) const {
    return static_cast<float>(mCtx.getLineLength(uiFont(mCtx), s, scale, false));
}

void Painter::texture(std::string const& path, core::Rect const& r, float alpha) {
    mCmds->push_back({Layer::Foreground, TextureCmd{path, r, alpha}});
}

core::Rect Painter::screen() const {
    auto gui = mCtx.mClient.getGuiData();
    auto sz  = gui->mScreenSizeData->clientUIScreenSize;
    return {0, 0, sz->x, sz->y};
}

void Painter::flush() {
    if (mCmds->empty()) return;
    static HashedString const fillMaterial{"ui_fillColor"};
    static HashedString const textureMaterial{"ui_textured_and_glcolor"};
    mce::Color const          white{1.f, 1.f, 1.f, 1.f};

    auto rects = [&](Layer layer) {
        bool any = false;
        for (auto& cmd : *mCmds) {
            if (cmd.layer != layer) continue;
            if (auto* rc = std::get_if<RectCmd>(&cmd.v)) {
                mCtx.fillRectangle(toArea(rc->r), toMce(rc->c), rc->c.a);
                any = true;
            }
        }
        if (any) mCtx.flushImages(white, 1.f, fillMaterial);
        bool anyTex = false;
        for (auto& cmd : *mCmds) {
            if (cmd.layer != layer) continue;
            if (auto* tc = std::get_if<TextureCmd>(&cmd.v)) {
                auto tex  = mCtx.getTexture(ResourceLocation(Core::PathView(tc->path)), false);
                auto data = tex.mClientTexture.get();
                if (!data) continue;
                mCtx.drawImage(data->mClientTexture, {tc->r.x, tc->r.y}, {tc->r.w, tc->r.h}, {0.f, 0.f}, {1.f, 1.f}, false);
                anyTex = true;
            }
        }
        if (anyTex) mCtx.flushImages(white, 1.f, textureMaterial);
    };

    rects(Layer::Background);

    // Items.
    if (auto* renderer = mCtx.mClient.getItemRenderer()) {
        BaseActorRenderContext rc(mCtx.mScreenContext, mCtx.mClient, mCtx.mClient.getMinecraftGame_DEPRECATED());
        auto&                  catalog = game::ItemCatalog::get();
        for (auto& cmd : *mCmds) {
            auto* ic = std::get_if<ItemCmd>(&cmd.v);
            if (!ic) continue;
            ItemStack const* stack = ic->stack ? ic->stack : catalog.displayStack(ic->key);
            if (!stack || stack->isNull()) continue;
            renderer->renderGuiItemNew(rc, *stack, 0, ic->x, ic->y, false, ic->alpha, 1.f, ic->scale, 0);
            if (stack->mItem && stack->mItem->isGlint(*stack)) {
                renderer->renderGuiItemNew(rc, *stack, 0, ic->x, ic->y, true, ic->alpha, 1.f, ic->scale, 0);
            }
        }
    }

    rects(Layer::Foreground);

    // Text.
    bool anyText = false;
    auto& font   = uiFont(mCtx);
    for (auto& cmd : *mCmds) {
        auto* tc = std::get_if<TextCmd>(&cmd.v);
        if (!tc) continue;
        float w  = tc->width > 0 ? tc->width : textWidth(tc->s, tc->scale) + 2;
        float x0 = tc->x;
        auto  al = ::ui::TextAlignment::Left;
        if (tc->align == TextAlign::Center) al = ::ui::TextAlignment::Center;
        if (tc->align == TextAlign::Right) al = ::ui::TextAlignment::Right;
        if (tc->width <= 0 && tc->align == TextAlign::Center) x0 -= w / 2;
        if (tc->width <= 0 && tc->align == TextAlign::Right) x0 -= w;
        RectangleArea   area{x0, x0 + w, tc->y, tc->y + lineHeight(tc->scale) + 2};
        TextMeasureData measure{tc->scale, 0.f, tc->c.r > 0.9f && tc->c.g > 0.9f, false, false, al};
        CaretMeasureData caret{-1, false};
        mCtx.drawText(font, area, std::string{tc->s}, toMce(tc->c), tc->c.a, al, measure, caret);
        anyText = true;
    }
    if (anyText) mCtx.flushText(0.f, std::nullopt);

    mCmds->clear();
}

} // namespace bei::ui
