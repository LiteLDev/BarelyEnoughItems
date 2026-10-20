#pragma once

#include "ui/UiLayer.h"

namespace bei::ui {

/// Bookmark list on the left of container screens: app::Controller::bookmarks() in a paged
/// grid with the same interactions as ItemListPanel. Visible together with the item list when
/// Config::bookmarksEnabled and there is at least one bookmark.
class BookmarkPanel final : public IUiLayer {
public:
    static BookmarkPanel& get();

    [[nodiscard]] char const* name() const override { return "bookmarks"; }
    [[nodiscard]] int         zOrder() const override { return 90; }
    [[nodiscard]] bool        visible(FrameInfo const& frame) const override;
    void                      render(Painter& painter, FrameInfo const& frame) override;

    bool onMouseButton(MouseButton button, bool down, float x, float y) override;
    bool onScroll(float x, float y, float delta) override;
    [[nodiscard]] bool contains(float x, float y) const override;
    [[nodiscard]] std::optional<core::ItemKey> itemAt(float x, float y) const override;
};

} // namespace bei::ui
