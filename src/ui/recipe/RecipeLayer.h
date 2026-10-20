#pragma once

// Internal to the recipe screen: the IUiLayer that draws the recipes GUI and handles its input.
// It is only visible while ui::RecipeScreen renders its own scene (see RecipeScreen.cpp).

#include "actions/Actions.h"
#include "core/model/RecipeView.h"
#include "core/state/NavHistory.h"
#include "ui/UiLayer.h"
#include "ui/recipe/RecipeDrawers.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace bei::ui::recipe {

class RecipeLayer final : public IUiLayer {
public:
    static RecipeLayer& get();

    /// (Re)target on a focus; tab/page are taken from focus.category/categoryTab/page.
    void open(core::RecipeFocus const& focus);
    /// Forget everything (screen closed).
    void reset();
    /// Set by RecipeScreen while its own scene renders: the layer is visible only then.
    void setRendering(bool rendering) noexcept { mRendering = rendering; }

    /// Screen keys (Windows VK): ESC, Backspace, arrows, PageUp/PageDown. True if handled.
    bool handleKey(int vk);
    [[nodiscard]] static bool isScreenKey(int vk) noexcept;

    // IUiLayer
    [[nodiscard]] char const* name() const override { return "recipe_screen"; }
    [[nodiscard]] int         zOrder() const override { return 1000; }
    [[nodiscard]] bool        visible(FrameInfo const& frame) const override;
    void                      render(Painter& painter, FrameInfo const& frame) override;
    bool                      onMouseButton(MouseButton button, bool down, float x, float y) override;
    bool                      onScroll(float x, float y, float delta) override;
    [[nodiscard]] bool        contains(float x, float y) const override;
    [[nodiscard]] std::optional<core::ItemKey> itemAt(float x, float y) const override;

private:
    struct Tab {
        core::CategoryInfo       info;
        std::vector<std::size_t> recipes; ///< Indices into RecipeIndex::recipe().
        core::ItemKey            icon;
    };

    bool refresh(); ///< Rebuild tabs if the focus or the recipe index changed. False if nothing to show.
    void selectTab(int index);
    void stepPage(int delta);
    void persist();
    void measureCell(Painter& p);
    [[nodiscard]] int  pageCount() const;
    [[nodiscard]] actions::TransferCheck const& transferCheck(std::size_t recipe, core::RecipeView const& view, double time);

    [[nodiscard]] DrawContext makeContext(Painter& p, FrameInfo const& frame);
    void layoutFrame(Painter& p, FrameInfo const& frame);
    void drawTabs(Painter& p, FrameInfo const& frame);
    void drawHeader(Painter& p, FrameInfo const& frame);
    void drawRecipes(Painter& p, FrameInfo const& frame);
    void drawWorkstations(Painter& p, FrameInfo const& frame);
    void drawTooltips(Painter& p, FrameInfo const& frame);
    void logStateIfChanged(FrameInfo const& frame); ///< Also logs the shown tab on change.

    // Focus and data.
    core::RecipeFocus mFocus;
    std::vector<Tab>  mTabs;
    int               mSel        = 0;
    int               mPage       = 0;
    int               mPerPage    = 1;
    std::uint64_t     mGeneration = ~0ull;
    bool              mInitial    = true; ///< Next refresh applies the focus' tab/page.
    bool              mRendering  = false;
    std::atomic<bool> mHasContent{false}; ///< Something is shown (read on the window thread).
    bool              mCellValid  = false;
    Size              mCell;
    float             mNoteW = 0; ///< Widest note line of the selected tab (measured with mCell).

    // Transfer checks, refreshed a few times per second.
    std::unordered_map<std::size_t, actions::TransferCheck> mChecks;
    double                                                  mChecksTime = -1;

    // Geometry of the last frame (GUI units), for hit testing.
    struct TabHit {
        core::Rect rect;
        int        index;
    };
    struct PlusHit {
        core::Rect  rect;
        std::size_t recipe;
        bool        canCraft;
    };
    core::Rect           mPanel, mTabStrip, mArea, mTitle;
    core::Rect           mCatPrev, mCatNext, mPagePrev, mPageNext, mClose;
    int                  mFirstTab = 0, mTabsShown = 0;
    std::vector<TabHit>  mTabHits;
    std::vector<SlotHit> mSlots;
    std::vector<HintHit> mHints;
    std::vector<PlusHit> mPlus;
    core::Rect           mLoggedPlus;            ///< Last logged first "+" (logStateIfChanged).
    bool                 mLoggedPlusReady  = false;
    std::size_t          mLoggedPlusRecipe = static_cast<std::size_t>(-1); ///< -1: nothing logged yet.
    std::string          mLoggedTab;             ///< Last logged tab (category id).
};

} // namespace bei::ui::recipe
