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
#include "mc/client/gui/controls/UIMaterialType.h"
#include "mc/client/gui/controls/renderers/InventoryItemRenderer.h"
#include "mc/client/gui/screens/BatchClippingState.h"
#include "mc/client/gui/screens/BatchKey.h"
#include "mc/client/gui/screens/ComponentRenderBatch.h"
#include "mc/client/gui/screens/UIBatchType.h"
#include "mc/client/renderer/actor/ItemRenderChunkType.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <unordered_map>
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
    float       u0 = 0, v0 = 0, u1 = 1, v1 = 1; // normalized UV rectangle
    float       alpha = 1;
};

// Vanilla resource-pack textures used for panels and slots (the same art the game's own
// container screens use), with rectangle fallbacks if a resource pack removes them.
constexpr char const* PANEL_TEXTURE = "textures/ui/dialog_background_opaque";
constexpr float       PANEL_TEX_SIZE = 16; // texture is 16x16 with a 4 px border
constexpr float       PANEL_SLICE    = 4;
constexpr char const* SLOT_TEXTURE   = "textures/ui/item_cell";

mce::Color toMce(Color c) { return mce::Color{c.r, c.g, c.b, c.a}; }

RectangleArea toArea(core::Rect const& r) { return RectangleArea{r.x, r.right(), r.y, r.bottom()}; }

Font& uiFont(MinecraftUIRenderContext& ctx) { return ctx.mClient.getFontHandle().getFont(); }

HashedString const& fillMaterial() {
    static HashedString const m{"ui_fillColor"};
    return m;
}
// Material the vanilla UI uses to flush textured quads.
HashedString const& imageMaterial() {
    static HashedString const m{"ui_flush"};
    return m;
}

// Render chunk types the vanilla slot renderer batches (InventoryItemRenderer::getRenderTypeFromItem):
// block models on the terrain atlas and flat sprites on the item atlas. Anything else (and any
// glinting stack) goes through ItemRenderer::renderGuiItemNew directly.
constexpr int CHUNK_BLOCK = 0;
constexpr int CHUNK_FLAT  = 2;

/// A ComponentRenderBatch set up like the one a vanilla inventory slot uses for its icons
/// (shared mesh, "Item" material, atlas texture). The type has no usable constructor, so it is
/// built in place in zeroed storage and destroyed with its own destructor.
class ItemMeshBatch {
public:
    ItemMeshBatch(int chunk, int depth, float alpha) {
        std::memset(mStorage, 0, sizeof(mStorage));
        auto& batch = get();
        alignas(BatchClippingState) std::byte clip[sizeof(BatchClippingState)]{}; // no clipping
        auto* key = ::new (std::addressof(batch.mBatchKey.get()))
            BatchKey(depth, alpha, *std::launder(reinterpret_cast<BatchClippingState*>(clip)));
        key->mBatchType      = UIBatchType::SharedMesh;
        key->mUIMaterialType = UIMaterialType::Item;
        auto& textures       = key->mResourceLocations.get();
        setLocation(textures[0], chunk == CHUNK_BLOCK ? "atlas.terrain" : "atlas.items");
        batch.mIsDirty                = true; // our icons change every frame
        batch.mRequiresPreRenderSetup = false;
        batch.mRenderPass             = 0;
        ::new (std::addressof(batch.mCustomRenderInstances.get())) std::vector<CustomRenderComponent*>();
        ::new (std::addressof(batch.mSpriteInstances.get())) std::vector<SpriteComponent*>();
        ::new (std::addressof(batch.mTextInstances.get())) std::vector<TextComponent*>();
    }
    ~ItemMeshBatch() { std::destroy_at(&get()); }
    ItemMeshBatch(ItemMeshBatch const&)            = delete;
    ItemMeshBatch& operator=(ItemMeshBatch const&) = delete;

    ComponentRenderBatch& get() { return *std::launder(reinterpret_cast<ComponentRenderBatch*>(mStorage)); }

private:
    static void setLocation(ResourceLocation& loc, char const* path) {
        std::destroy_at(&loc);
        ::new (&loc) ResourceLocation(Core::PathView(path));
    }
    alignas(ComponentRenderBatch) std::byte mStorage[sizeof(ComponentRenderBatch)];
};

/// Durability of a stack as remaining/max in [0,1], or a negative value if it has none.
float durabilityRatio(ItemStack const& stack) {
    if (!stack.mItem || !stack.isDamageableItem() || !stack.isDamaged()) return -1;
    short maxDamage = stack.mItem->getMaxDamage();
    if (maxDamage <= 0) return -1;
    float remaining = static_cast<float>(maxDamage - stack.getDamageValue());
    return std::clamp(remaining / static_cast<float>(maxDamage), 0.f, 1.f);
}
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

void Painter::panel(core::Rect const& r) { nineSlice(PANEL_TEXTURE, r, PANEL_TEX_SIZE, PANEL_TEX_SIZE, PANEL_SLICE); }

void Painter::slot(float x, float y, float size) {
    mCmds->push_back({Layer::Background, TextureCmd{SLOT_TEXTURE, {x, y, size, size}}});
}

void Painter::nineSlice(std::string const& path, core::Rect const& r, float texW, float texH, float slice, Layer layer) {
    if (r.empty() || texW <= 0 || texH <= 0) return;
    float s  = std::min({slice, r.w / 2, r.h / 2});
    float xs[4]{r.x, r.x + s, r.right() - s, r.right()};
    float ys[4]{r.y, r.y + s, r.bottom() - s, r.bottom()};
    float us[4]{0, slice / texW, 1 - slice / texW, 1};
    float vs[4]{0, slice / texH, 1 - slice / texH, 1};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            core::Rect part{xs[col], ys[row], xs[col + 1] - xs[col], ys[row + 1] - ys[row]};
            if (part.empty()) continue;
            mCmds->push_back({layer, TextureCmd{path, part, us[col], vs[row], us[col + 1], vs[row + 1]}});
        }
    }
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
    mCmds->push_back({Layer::Foreground, TextureCmd{path, r, 0, 0, 1, 1, alpha}});
}

core::Rect Painter::screen() const {
    auto gui = mCtx.mClient.getGuiData();
    auto sz  = gui->mScreenSizeData->clientUIScreenSize;
    return {0, 0, sz->x, sz->y};
}

void Painter::flush() {
    if (mCmds->empty()) return;
    mce::Color const white{1.f, 1.f, 1.f, 1.f};

    // Textures are resolved once per flush; a TexturePtr is never kept across frames.
    std::unordered_map<std::string, mce::TexturePtr> textures;
    auto clientTexture = [&](std::string const& path) -> BedrockTextureData const* {
        auto it = textures.find(path);
        if (it == textures.end()) {
            it = textures.emplace(path, mCtx.getTexture(ResourceLocation(Core::PathView(path)), false)).first;
        }
        return it->second.mClientTexture.get();
    };

    // Rectangles, then textured quads, of one layer. Panels/slots whose texture is missing
    // fall back to plain rectangles so the UI stays usable with unusual resource packs.
    auto quads = [&](Layer layer) {
        bool anyFill = false;
        auto fill    = [&](core::Rect const& r, Color c) {
            mCtx.fillRectangle(toArea(r), toMce(c), c.a);
            anyFill = true;
        };
        for (auto& cmd : *mCmds) {
            if (cmd.layer != layer) continue;
            if (auto* rc = std::get_if<RectCmd>(&cmd.v)) fill(rc->r, rc->c);
            if (auto* tc = std::get_if<TextureCmd>(&cmd.v); tc && !clientTexture(tc->path)) {
                if (tc->path == SLOT_TEXTURE) fill(tc->r, palette::SLOT_BG);
                else if (tc->path == PANEL_TEXTURE) fill(tc->r, palette::PANEL_BG);
            }
        }
        if (anyFill) mCtx.flushImages(white, 1.f, fillMaterial());

        // Group quads by alpha: flushImages applies one alpha to everything queued.
        std::vector<float> alphas;
        for (auto& cmd : *mCmds) {
            if (cmd.layer != layer) continue;
            if (auto* tc = std::get_if<TextureCmd>(&cmd.v);
                tc && std::find(alphas.begin(), alphas.end(), tc->alpha) == alphas.end()) {
                alphas.push_back(tc->alpha);
            }
        }
        for (float alpha : alphas) {
            bool any = false;
            for (auto& cmd : *mCmds) {
                if (cmd.layer != layer) continue;
                auto* tc = std::get_if<TextureCmd>(&cmd.v);
                if (!tc || tc->alpha != alpha) continue;
                auto const* data = clientTexture(tc->path);
                if (!data) continue;
                mCtx.drawImage(
                    data->mClientTexture,
                    {tc->r.x, tc->r.y},
                    {tc->r.w, tc->r.h},
                    {tc->u0, tc->v0},
                    {tc->u1 - tc->u0, tc->v1 - tc->v0},
                    false
                );
                any = true;
            }
            if (any) mCtx.flushImages(white, alpha, imageMaterial());
        }
    };

    quads(Layer::Background);

    // Items. Flush an (off-screen) quad first so no half-built image batch is pending when the
    // item renderer starts emitting meshes, then draw icons the way vanilla slots do.
    struct Resolved {
        ItemStack const* stack;
        ItemCmd const*   cmd;
    };
    std::vector<Resolved> items;
    auto&                 catalog = game::ItemCatalog::get();
    for (auto& cmd : *mCmds) {
        auto* ic = std::get_if<ItemCmd>(&cmd.v);
        if (!ic) continue;
        ItemStack const* stack = ic->stack ? ic->stack : catalog.displayStack(ic->key);
        if (!stack || stack->isNull() || !stack->mItem) continue;
        items.push_back({stack, ic});
    }
    auto* renderer = mCtx.mClient.getItemRenderer();
    if (renderer && !items.empty()) {
        mCtx.fillRectangle(RectangleArea{-10000.f, -9999.f, -10000.f, -9999.f}, white, 1.f);
        mCtx.flushImages(white, 1.f, imageMaterial());

        BaseActorRenderContext rc(mCtx.mScreenContext, mCtx.mClient, mCtx.mClient.getMinecraftGame_DEPRECATED());
        auto direct = [&](Resolved const& it, bool foil) {
            auto const& c = *it.cmd;
            renderer->renderGuiItemNew(rc, *it.stack, 0, c.x, c.y, foil, c.alpha, 1.f, c.scale, 0);
        };
        std::vector<int> chunks(items.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            chunks[i] = items[i].stack->mItem->isGlint(*items[i].stack)
                          ? -1
                          : static_cast<int>(InventoryItemRenderer::getRenderTypeFromItem(*items[i].stack));
        }
        // Glinting stacks first, icon then foil overlay: after a shared-mesh batch the foil pass
        // draws nothing.
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (chunks[i] != -1) continue;
            direct(items[i], false);
            direct(items[i], true);
        }
        // Block and flat icons in shared-mesh batches, one per (chunk type, alpha), exactly like
        // vanilla slots. Some block models (e.g. fence gates) only render through this path.
        std::vector<float> alphas;
        for (auto const& it : items) {
            if (std::find(alphas.begin(), alphas.end(), it.cmd->alpha) == alphas.end()) alphas.push_back(it.cmd->alpha);
        }
        for (int chunk : {CHUNK_BLOCK, CHUNK_FLAT}) {
            for (float alpha : alphas) {
                bool any = false;
                for (std::size_t i = 0; i < items.size() && !any; ++i) any = chunks[i] == chunk && items[i].cmd->alpha == alpha;
                if (!any) continue;
                ItemMeshBatch batch(chunk, 0, alpha);
                mCtx.beginSharedMeshBatch(batch.get());
                for (std::size_t i = 0; i < items.size(); ++i) {
                    if (chunks[i] != chunk || items[i].cmd->alpha != alpha) continue;
                    auto const& c = *items[i].cmd;
                    renderer->renderGuiItemInChunk(
                        rc,
                        static_cast<ItemRenderChunkType>(chunk),
                        *items[i].stack,
                        c.x,
                        c.y,
                        1.f,
                        chunk == CHUNK_BLOCK ? 0.f : 1.f, // what vanilla slots pass for each chunk type
                        c.scale,
                        0,
                        false,
                        0,
                        std::nullopt
                    );
                }
                mCtx.endSharedMeshBatch(batch.get());
            }
        }
        // Everything else (banners, shields, heads, ...) via the direct path.
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (chunks[i] != -1 && chunks[i] != CHUNK_BLOCK && chunks[i] != CHUNK_FLAT) direct(items[i], false);
        }

        // Durability bars (13x2 at the bottom of the 16x16 icon, like vanilla), drawn above icons.
        for (auto const& it : items) {
            float ratio = durabilityRatio(*it.stack);
            if (ratio < 0) continue;
            auto const& c = *it.cmd;
            float       x = c.x + 2 * c.scale, y = c.y + 13 * c.scale;
            fillRect({x, y, 13 * c.scale, 2 * c.scale}, Color::rgb(0x000000), Layer::Foreground);
            fillRect({x, y, 13 * c.scale * ratio, 1 * c.scale}, Color{1 - ratio, ratio, 0, 1}, Layer::Foreground);
        }
    }

    quads(Layer::Foreground);

    // Text.
    bool  anyText = false;
    auto& font    = uiFont(mCtx);
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
        RectangleArea    area{x0, x0 + w, tc->y, tc->y + lineHeight(tc->scale) + 2};
        TextMeasureData  measure{tc->scale, 0.f, tc->c.r > 0.9f && tc->c.g > 0.9f, false, false, al};
        CaretMeasureData caret{-1, false};
        mCtx.drawText(font, area, std::string{tc->s}, toMce(tc->c), tc->c.a, al, measure, caret);
        anyText = true;
    }
    if (anyText) mCtx.flushText(0.f, std::nullopt);

    mCmds->clear();
}

} // namespace bei::ui
