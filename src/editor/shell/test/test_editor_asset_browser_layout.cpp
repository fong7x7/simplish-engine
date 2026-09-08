#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-asset-browser-layout.h>

using Catch::Approx;
using namespace eng::editor;

namespace {

/// A browser panel across the bottom of a 1280x900 window.
constexpr eng::Rect PANEL{0.0f, 680.0f, 1280.0f, ASSET_PANEL_HEIGHT};

}  // namespace

TEST_CASE("the header takes the top of the panel") {
  const EditorAssetBrowserLayout out = layoutAssetBrowser(PANEL);
  REQUIRE(out.header.x == Approx(PANEL.x));
  REQUIRE(out.header.y == Approx(PANEL.y));
  REQUIRE(out.header.w == Approx(PANEL.w));
  REQUIRE(out.header.h == Approx(ASSET_HEADER_HEIGHT));
}

TEST_CASE("the folder pane takes the left of what is below the header") {
  const EditorAssetBrowserLayout out = layoutAssetBrowser(PANEL);
  REQUIRE(out.nav.x == Approx(PANEL.x));
  REQUIRE(out.nav.y == Approx(PANEL.y + ASSET_HEADER_HEIGHT));
  REQUIRE(out.nav.w == Approx(ASSET_NAV_WIDTH));
  REQUIRE(out.nav.h == Approx(PANEL.h - ASSET_HEADER_HEIGHT));
}

TEST_CASE("the grid takes what the folder pane leaves") {
  const EditorAssetBrowserLayout out = layoutAssetBrowser(PANEL);
  REQUIRE(out.grid.x == Approx(out.nav.x + out.nav.w));
  REQUIRE(out.grid.w == Approx(PANEL.w - ASSET_NAV_WIDTH));
  REQUIRE(out.grid.y == Approx(out.nav.y));
  REQUIRE(out.grid.h == Approx(out.nav.h));
}

TEST_CASE("the regions never overlap") {
  const EditorAssetBrowserLayout out = layoutAssetBrowser(PANEL);
  REQUIRE(intersectRects(out.header, out.nav).h == Approx(0.0f));
  REQUIRE(intersectRects(out.nav, out.grid).w == Approx(0.0f));
}

TEST_CASE("a panel narrower than the folder pane hands out no negatives") {
  const EditorAssetBrowserLayout out =
      layoutAssetBrowser({0.0f, 0.0f, 80.0f, ASSET_PANEL_HEIGHT});
  // The pane takes what there is and the grid gets nothing, rather than
  // either being handed a width below zero.
  REQUIRE(out.nav.w == Approx(80.0f));
  REQUIRE(out.grid.w == Approx(0.0f));
}

TEST_CASE("a panel shorter than its header hands out no negatives") {
  const EditorAssetBrowserLayout out =
      layoutAssetBrowser({0.0f, 0.0f, 1280.0f, 8.0f});
  REQUIRE(out.nav.h == Approx(0.0f));
  REQUIRE(out.grid.h == Approx(0.0f));
}

TEST_CASE("folder rows stack down the pane") {
  const eng::Rect nav = layoutAssetBrowser(PANEL).nav;
  const eng::Rect first = assetFolderRowRect(nav, 0, 0.0f);
  const eng::Rect second = assetFolderRowRect(nav, 1, 0.0f);
  REQUIRE(first.y == Approx(nav.y));
  REQUIRE(second.y == Approx(first.y + ASSET_ROW_HEIGHT));
  REQUIRE(first.w == Approx(nav.w));
}

TEST_CASE("scrolling the pane lifts its rows") {
  const eng::Rect nav = layoutAssetBrowser(PANEL).nav;
  const eng::Rect unscrolled = assetFolderRowRect(nav, 4, 0.0f);
  const eng::Rect scrolled = assetFolderRowRect(nav, 4, 30.0f);
  REQUIRE(scrolled.y == Approx(unscrolled.y - 30.0f));
}

TEST_CASE("nesting indents a row's chevron and label") {
  const eng::Rect row =
      assetFolderRowRect(layoutAssetBrowser(PANEL).nav, 0, 0.0f);
  const eng::Rect shallow = assetFolderChevronRect(row, 0);
  const eng::Rect deep = assetFolderChevronRect(row, 2);
  REQUIRE(deep.x == Approx(shallow.x + (2.0f * ASSET_ROW_INDENT)));
  REQUIRE(assetFolderLabelX(row, 2) == Approx(deep.x + ASSET_CHEVRON_WIDTH));
}

TEST_CASE("cards run left to right before wrapping") {
  const eng::Rect grid = layoutAssetBrowser(PANEL).grid;
  const eng::Rect first = assetCardRect(grid, 0, 0.0f);
  const eng::Rect second = assetCardRect(grid, 1, 0.0f);
  REQUIRE(second.x == Approx(first.x + ASSET_CARD_WIDTH + ASSET_CARD_GAP));
  REQUIRE(second.y == Approx(first.y));
  REQUIRE(first.w == Approx(ASSET_CARD_WIDTH));
  REQUIRE(first.h == Approx(ASSET_CARD_HEIGHT));
}

TEST_CASE("a card past the end of a row wraps to the next one") {
  const eng::Rect grid = layoutAssetBrowser(PANEL).grid;
  const size_t per_row = assetCardsPerRow(grid);
  const eng::Rect first = assetCardRect(grid, 0, 0.0f);
  const eng::Rect wrapped = assetCardRect(grid, per_row, 0.0f);
  REQUIRE(wrapped.x == Approx(first.x));
  REQUIRE(wrapped.y == Approx(first.y + ASSET_CARD_HEIGHT + ASSET_CARD_GAP));
}

TEST_CASE("every card in a row stays inside the grid") {
  const eng::Rect grid = layoutAssetBrowser(PANEL).grid;
  const size_t per_row = assetCardsPerRow(grid);
  const eng::Rect last = assetCardRect(grid, per_row - 1, 0.0f);
  REQUIRE(last.x + last.w <= grid.x + grid.w);
}

TEST_CASE("a grid too narrow for a card still lays out one per row") {
  // The alternative is dividing by zero when working out the column.
  REQUIRE(assetCardsPerRow({0.0f, 0.0f, 10.0f, 100.0f}) == 1);
}

TEST_CASE("scrolling the grid lifts its cards") {
  const eng::Rect grid = layoutAssetBrowser(PANEL).grid;
  const eng::Rect unscrolled = assetCardRect(grid, 0, 0.0f);
  const eng::Rect scrolled = assetCardRect(grid, 0, 40.0f);
  REQUIRE(scrolled.y == Approx(unscrolled.y - 40.0f));
}

TEST_CASE("an empty grid has no content to scroll") {
  const eng::Rect grid = layoutAssetBrowser(PANEL).grid;
  REQUIRE(assetGridContentHeight(grid, 0) == Approx(0.0f));
}

TEST_CASE("grid content height covers the last row in full") {
  const eng::Rect grid = layoutAssetBrowser(PANEL).grid;
  const size_t per_row = assetCardsPerRow(grid);
  // One card past a full row still needs the whole second row's height.
  REQUIRE(assetGridContentHeight(grid, per_row + 1) ==
          Approx(assetGridContentHeight(grid, per_row * 2)));
}

TEST_CASE("pane content height is one row per entry") {
  REQUIRE(assetNavContentHeight(5) == Approx(5.0f * ASSET_ROW_HEIGHT));
}

TEST_CASE("content that fits cannot be scrolled") {
  REQUIRE(clampAssetScroll(120.0f, 50.0f, 198.0f) == Approx(0.0f));
}

TEST_CASE("scrolling stops at the end of the content") {
  REQUIRE(clampAssetScroll(500.0f, 300.0f, 198.0f) == Approx(102.0f));
}

TEST_CASE("scrolling stops at the top of the content") {
  REQUIRE(clampAssetScroll(-40.0f, 300.0f, 198.0f) == Approx(0.0f));
}
