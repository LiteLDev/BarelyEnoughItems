// Bookmark panel: the bookmarked items in a paged grid on the left of container screens.
#include "ui/overlay/BookmarkPanel.h"

#include "app/Controller.h"
#include "core/state/Layout.h"
#include "ui/UiRoot.h"
#include "ui/overlay/PagedItemGrid.h"

namespace bei::ui {

namespace {
PagedItemGrid gGrid;
bool          gInitialized = false;

std::vector<core::ItemKey> const& items() { return app::Controller::get().bookmarks().items(); }
} // namespace

BookmarkPanel& BookmarkPanel::get() {
    static BookmarkPanel instance;
    return instance;
}

bool BookmarkPanel::visible(FrameInfo const& frame) const {
    return overlayAllowed(frame) && app::Controller::get().config().bookmarksEnabled && !items().empty();
}

void BookmarkPanel::render(Painter& p, FrameInfo const& frame) {
    if (!gInitialized) {
        gInitialized = true;
        gGrid.keyAt  = [](std::size_t i) -> core::ItemKey const* {
            auto const& list = items();
            return i < list.size() ? &list[i] : nullptr;
        };
    }
    auto layout = core::computeOverlayLayout(frame.screen.w, frame.screen.h, frame.container);
    if (!gGrid.layout(layout.bookmarkPanel, 0, app::Controller::get().config().maxColumns)) return;
    gGrid.logLayoutIfChanged("Bookmark panel", frame.screen);
    gGrid.setItemCount(items().size());
    gGrid.render(p, frame, {});
    if (auto item = gGrid.itemAt(frame.mouseX, frame.mouseY)) requestItemTooltip(*item);
}

bool BookmarkPanel::onMouseButton(MouseButton button, bool down, float x, float y) {
    return gGrid.onMouseButton(button, down, x, y);
}

bool BookmarkPanel::onScroll(float x, float y, float delta) { return gGrid.onScroll(x, y, delta); }

bool BookmarkPanel::contains(float x, float y) const { return gGrid.contains(x, y); }

std::optional<core::ItemKey> BookmarkPanel::itemAt(float x, float y) const { return gGrid.itemAt(x, y); }

BEI_REGISTER_UI_LAYER(BookmarkPanel::get());

} // namespace bei::ui
