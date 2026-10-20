#pragma once

#include "ui/UiLayer.h"

namespace bei::ui {

/// The item list panel shown to the right of container screens (search bar, item grid,
/// page arrows) and the bookmark panel on the left.
///
/// Contract:
/// - Visible on container screens when Config::overlayEnabled (bookmarks: bookmarksEnabled),
///   hidden while the recipe screen is open unless the recipe screen asks for it.
/// - itemAt() reports the hovered item so R/U/A hotkeys work on the panel.
/// - Left click on an item: cheat mode -> actions::Actions::give, otherwise showRecipes;
///   right click: showUses. Wheel scrolls pages.
/// - The search text lives in app::Controller (searchText/setSearchText).
class ItemListPanel final : public IUiLayer {
public:
    static ItemListPanel& get();

    [[nodiscard]] char const* name() const override { return "item_list"; }
    [[nodiscard]] int         zOrder() const override { return 100; }
    [[nodiscard]] bool        visible(FrameInfo const& frame) const override;
    void                      render(Painter& painter, FrameInfo const& frame) override;

    bool onMouseButton(MouseButton button, bool down, float x, float y) override;
    bool onScroll(float x, float y, float delta) override;
    bool onKey(int vk, bool down) override;
    bool onChar(std::string const& utf8) override;
    [[nodiscard]] bool wantsTextInput() const override;
    [[nodiscard]] bool contains(float x, float y) const override;
    [[nodiscard]] std::optional<core::ItemKey> itemAt(float x, float y) const override;
};

} // namespace bei::ui
