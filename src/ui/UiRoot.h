#pragma once

#include "ui/UiLayer.h"

#include <memory>
#include <vector>

class ScreenView;
class MinecraftUIRenderContext;

namespace bei::ui {

/// Hosts all IUiLayers: renders them after the focused screen's UI pass and dispatches
/// input to them (dispatch* are called by input::InputRouter on the client thread).
class UiRoot {
public:
    static UiRoot& get();

    void install();
    void uninstall();

    void add(IUiLayer* layer); ///< Static-init registration; layers are never removed.

    [[nodiscard]] FrameInfo const& lastFrame() const noexcept { return mFrame; }
    void setMouse(float x, float y) noexcept { mFrame.mouseX = x, mFrame.mouseY = y; }

    bool dispatchMouseButton(MouseButton button, bool down, float x, float y);
    bool dispatchScroll(float x, float y, float delta);
    bool dispatchKey(int vk, bool down);
    bool dispatchChar(std::string const& utf8);
    [[nodiscard]] bool wantsTextInput() const;
    [[nodiscard]] bool contains(float x, float y) const;
    [[nodiscard]] std::optional<core::ItemKey> itemAt(float x, float y) const;

    /// Called from the render hook. Public for the recipe screen, which renders inside its own scene.
    void renderFrame(ScreenView& view, MinecraftUIRenderContext& ctx);

private:
    std::vector<IUiLayer*> sortedVisible() const;

    std::vector<IUiLayer*> mLayers;
    std::vector<IUiLayer*> mVisible; // visible in the last frame, descending zOrder
    FrameInfo              mFrame;
};

} // namespace bei::ui

/// Register a layer instance with static storage duration: BEI_REGISTER_UI_LAYER(gMyLayer);
#define BEI_REGISTER_UI_LAYER(INSTANCE)                                                                                \
    [[maybe_unused]] static bool const BEI_UI_CONCAT(beiUiLayerRegistered_, __LINE__) = [] {                           \
        ::bei::ui::UiRoot::get().add(&(INSTANCE));                                                                     \
        return true;                                                                                                   \
    }()
#define BEI_UI_CONCAT_IMPL(a, b) a##b
#define BEI_UI_CONCAT(a, b)      BEI_UI_CONCAT_IMPL(a, b)
