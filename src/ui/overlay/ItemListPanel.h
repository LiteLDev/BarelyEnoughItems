#pragma once

#include "ui/UiLayer.h"

#include <functional>

namespace bei::ui {

/// The item list panel shown to the right of container screens (search bar, item grid,
/// page arrows) and the bookmark panel on the left (BookmarkPanel, same interactions).
///
/// Contract:
/// - Visible on container screens when Config::overlayEnabled (bookmarks: bookmarksEnabled),
///   hidden while the recipe screen is open unless the recipe screen asks for it.
/// - itemAt() reports the hovered item so R/U/A hotkeys work on the panel.
/// - Left click on an item: cheat mode -> actions::Actions::give, otherwise showRecipes;
///   right click: showUses. Wheel scrolls pages.
/// - The search text lives in app::Controller (searchText/setSearchText).
/// - The page is kept when the item catalog changes and reset to the first page when the
///   search query changes.
class ItemListPanel final : public IUiLayer {
public:
    static ItemListPanel& get();

    /// Optional hook for the actions unit: called (client thread) for every mouse button event
    /// over the item grids of BEI's panels (item list and bookmarks) while cheat mode is active,
    /// before BEI handles it. Return true to take the event over (e.g. the vanilla cursor holds
    /// an item that should be deleted); BEI then does nothing else with it. Note that
    /// input::InputRouter hides clicks over BEI panels from vanilla either way.
    static inline std::function<bool(MouseButton button, bool down, float x, float y)> cheatClickHook;

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
