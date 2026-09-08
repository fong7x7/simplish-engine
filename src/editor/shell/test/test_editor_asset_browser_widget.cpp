#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-asset-browser-widget.h>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Catch::Approx;
using namespace eng::editor;

namespace {

constexpr eng::Rect PANEL_RECT{0.0f, 680.0f, 1280.0f, ASSET_PANEL_HEIGHT};

/// An asset at a relative path, as the scan would have produced it.
EditorAsset makeAsset(const std::string& relative) {
  const fs::path path(relative);
  return {path.stem().string(), fs::path("/project/assets") / path, path};
}

/// A browser listing the given relative asset paths.
EditorAssetBrowserWidget makeBrowser(const std::vector<std::string>& paths) {
  EditorAssetScan scan;
  for (const std::string& path : paths) {
    scan.assets.push_back(makeAsset(path));
  }
  std::vector<std::string> names;
  for (const EditorAsset& asset : scan.assets) {
    names.push_back(asset.name);
  }
  EditorAssetBrowserWidget browser;
  browser.rect = PANEL_RECT;
  browser.setAssets(buildEditorAssetTree(scan), std::move(names));
  return browser;
}

eng::GuiMouseEvent mouseAt(float x, float y) {
  eng::GuiMouseEvent event{};
  event.x = x;
  event.y = y;
  event.button = eng::GuiMouseButton::LEFT;
  return event;
}

/// Centre of a rect, as a press.
eng::GuiMouseEvent centreOf(const eng::Rect& rect) {
  return mouseAt(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f);
}

eng::GuiScrollEvent scrollAt(float x, float y, float delta) {
  eng::GuiScrollEvent event{};
  event.x = x;
  event.y = y;
  event.delta_y = delta;
  return event;
}

}  // namespace

TEST_CASE("the browser opens on the root folder") {
  const EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "props/barrel.obj"});

  REQUIRE(browser.selectedFolder() == EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(browser.assetCount() == 2);
}

TEST_CASE("the grid shows only the selected folder's assets") {
  const EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "props/barrel.obj", "props/crate.obj"});

  // The root holds one of the three; the other two are filed away.
  REQUIRE(browser.visibleAssets().size() == 1);
}

TEST_CASE("the root's children are listed from the start") {
  const EditorAssetBrowserWidget browser =
      makeBrowser({"props/barrel.obj", "terrain/mud.obj"});

  // Opening onto a pane showing one collapsed row would hide the whole
  // point of the folders.
  REQUIRE(browser.folderRows().size() == 3);
  REQUIRE(browser.folderExpanded(EDITOR_ASSET_FOLDER_ROOT));
}

TEST_CASE("clicking a folder row shows that folder's assets") {
  EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "props/barrel.obj", "props/keg.obj"});
  const size_t props = browser.folderRows()[1].folder;

  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));

  REQUIRE(browser.selectedFolder() == props);
  REQUIRE(browser.visibleAssets().size() == 2);
}

TEST_CASE("clicking a chevron opens a folder without selecting it") {
  EditorAssetBrowserWidget browser = makeBrowser({"terrain/rocks/boulder.obj"});
  const EditorAssetFolderRow terrain = browser.folderRows()[1];
  const eng::Rect chevron =
      assetFolderChevronRect(browser.folderRowRect(1), terrain.depth);

  browser.handleMouseDown(centreOf(chevron));

  REQUIRE(browser.folderExpanded(terrain.folder));
  REQUIRE(browser.folderRows().size() == 3);
  REQUIRE(browser.selectedFolder() == EDITOR_ASSET_FOLDER_ROOT);
}

TEST_CASE("clicking an open folder's chevron shuts it again") {
  EditorAssetBrowserWidget browser = makeBrowser({"props/barrel.obj"});
  const eng::Rect chevron = assetFolderChevronRect(browser.folderRowRect(0), 0);

  browser.handleMouseDown(centreOf(chevron));

  REQUIRE_FALSE(browser.folderExpanded(EDITOR_ASSET_FOLDER_ROOT));
  REQUIRE(browser.folderRows().size() == 1);
}

TEST_CASE("a folder holding only folders selects to an empty grid") {
  EditorAssetBrowserWidget browser = makeBrowser({"terrain/rocks/boulder.obj"});

  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));

  REQUIRE(browser.selectedFolder() != EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(browser.visibleAssets().empty());
}

TEST_CASE("cards are laid out inside the grid, not over the folder pane") {
  const EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const eng::Rect card = browser.cardRect(0);
  const eng::Rect nav = browser.layout().nav;

  REQUIRE(card.x >= nav.x + nav.w);
}

TEST_CASE("pressing a card is what starts a drag") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});

  REQUIRE(browser.handleMouseDown(centreOf(browser.cardRect(0))));
  REQUIRE(browser.draggingIndex() == 0);
}

TEST_CASE("pressing the grid's empty space starts nothing") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const eng::Rect grid = browser.layout().grid;

  REQUIRE_FALSE(browser.handleMouseDown(
      mouseAt(grid.x + grid.w - 4.0f, grid.y + grid.h - 4.0f)));
  REQUIRE(browser.draggingIndex() == -1);
}

TEST_CASE("a drop outside the panel reports the asset's place in the list") {
  EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "props/barrel.obj", "props/keg.obj"});
  size_t dropped = 0;
  browser.on_asset_dropped = [&](size_t index, float, float) {
    dropped = index;
  };
  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));

  browser.handleMouseDown(centreOf(browser.cardRect(1)));
  browser.handleMouseUp(mouseAt(600.0f, 300.0f));

  // "keg" is the third asset overall, and that is the number the editor
  // places — not the second card in the folder that happens to show it.
  REQUIRE(dropped == 2);
}

TEST_CASE("a drop back inside the panel places nothing") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  bool dropped = false;
  browser.on_asset_dropped = [&](size_t, float, float) {
    dropped = true;
  };

  browser.handleMouseDown(centreOf(browser.cardRect(0)));
  browser.handleMouseUp(centreOf(browser.cardRect(0)));

  REQUIRE_FALSE(dropped);
  REQUIRE(browser.draggingIndex() == -1);
}

TEST_CASE("a drag is reported once and only once") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  int drops = 0;
  browser.on_asset_dropped = [&](size_t, float, float) {
    ++drops;
  };

  browser.handleMouseDown(centreOf(browser.cardRect(0)));
  browser.handleMouseUp(mouseAt(600.0f, 300.0f));
  browser.handleMouseUp(mouseAt(600.0f, 300.0f));

  REQUIRE(drops == 1);
}

TEST_CASE("the drag ghost follows the cursor") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});

  browser.handleMouseDown(centreOf(browser.cardRect(0)));
  browser.handleMouseMove(mouseAt(400.0f, 200.0f));

  // Nothing to assert on the ghost's pixels here; what matters is that the
  // move did not end the drag.
  REQUIRE(browser.draggingIndex() == 0);
}

TEST_CASE("scrolling the grid moves the cards under the cursor") {
  std::vector<std::string> many;
  for (int i = 0; i < 60; ++i) {
    many.push_back("asset" + std::to_string(i) + ".obj");
  }
  EditorAssetBrowserWidget browser = makeBrowser(many);
  const float before = browser.cardRect(0).y;
  const eng::Rect grid = browser.layout().grid;

  browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, -1.0f));

  REQUIRE(browser.cardRect(0).y < before);
}

TEST_CASE("scrolling a grid whose cards all fit does nothing") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const float before = browser.cardRect(0).y;
  const eng::Rect grid = browser.layout().grid;

  browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, -4.0f));

  REQUIRE(browser.cardRect(0).y == Approx(before));
}

TEST_CASE("scrolling back past the top stops at the top") {
  std::vector<std::string> many;
  for (int i = 0; i < 60; ++i) {
    many.push_back("asset" + std::to_string(i) + ".obj");
  }
  EditorAssetBrowserWidget browser = makeBrowser(many);
  const float top = browser.cardRect(0).y;
  const eng::Rect grid = browser.layout().grid;

  browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, -2.0f));
  browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, 20.0f));

  REQUIRE(browser.cardRect(0).y == Approx(top));
}

TEST_CASE("a scrolled card is pressed where it is drawn") {
  std::vector<std::string> many;
  for (int i = 0; i < 60; ++i) {
    many.push_back("asset" + std::to_string(i) + ".obj");
  }
  EditorAssetBrowserWidget browser = makeBrowser(many);
  const eng::Rect grid = browser.layout().grid;
  browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, -1.0f));

  const size_t slot = assetCardsPerRow(grid);
  const eng::Rect card = browser.cardRect(slot);

  REQUIRE(browser.hitTestCard(card.x + 2.0f, card.y + 2.0f) ==
          static_cast<int>(slot));
}

TEST_CASE("scrolling outside either pane is not consumed") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});

  REQUIRE_FALSE(browser.handleScroll(scrollAt(600.0f, 100.0f, -1.0f)));
}

TEST_CASE("a point in the gap between cards hits neither") {
  const EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "barrel.obj"});
  const eng::Rect first = browser.cardRect(0);

  REQUIRE(browser.hitTestCard(first.x + first.w + 2.0f, first.y + 4.0f) == -1);
}

TEST_CASE("rescanning returns the browser to the root") {
  EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "props/barrel.obj"});
  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));
  REQUIRE(browser.selectedFolder() != EDITOR_ASSET_FOLDER_ROOT);

  browser = makeBrowser({"crate.obj"});

  REQUIRE(browser.selectedFolder() == EDITOR_ASSET_FOLDER_ROOT);
}

TEST_CASE("a browser with no assets lists the root and nothing else") {
  const EditorAssetBrowserWidget browser = makeBrowser({});

  REQUIRE(browser.folderRows().size() == 1);
  REQUIRE(browser.visibleAssets().empty());
  REQUIRE(browser.assetCount() == 0);
}

TEST_CASE("a default-constructed browser is safe to query") {
  // The editor builds the chrome before any project is open.
  const EditorAssetBrowserWidget browser;

  REQUIRE(browser.selectedFolder() == EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(browser.visibleAssets().empty());
  REQUIRE(browser.folderRows().size() == 1);
}

TEST_CASE("a browser handed a tree with no root repairs it") {
  EditorAssetBrowserWidget browser;
  EditorAssetTree tree;
  tree.folders.clear();

  browser.setAssets(std::move(tree), {});

  REQUIRE(browser.folderRows().size() == 1);
  REQUIRE(browser.visibleAssets().empty());
}

TEST_CASE("selecting a folder that does not exist changes nothing") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});

  browser.selectFolder(99);

  REQUIRE(browser.selectedFolder() == EDITOR_ASSET_FOLDER_ROOT);
}

TEST_CASE("the browser opens with both panes showing") {
  const EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});

  REQUIRE_FALSE(browser.navCollapsed());
  REQUIRE_FALSE(browser.panelCollapsed());
  REQUIRE(browser.preferredHeight() == Approx(ASSET_PANEL_HEIGHT));
}

TEST_CASE("the header's right-hand control folds the panel away") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const eng::Rect toggle = assetPanelToggleRect(browser.layout().header);

  browser.handleMouseDown(centreOf(toggle));

  REQUIRE(browser.panelCollapsed());
  REQUIRE(browser.preferredHeight() == Approx(ASSET_PANEL_COLLAPSED_HEIGHT));
}

TEST_CASE("the same control brings a folded panel back") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const eng::Rect toggle = assetPanelToggleRect(browser.layout().header);

  browser.handleMouseDown(centreOf(toggle));
  browser.handleMouseDown(centreOf(toggle));

  REQUIRE_FALSE(browser.panelCollapsed());
  REQUIRE(browser.preferredHeight() == Approx(ASSET_PANEL_HEIGHT));
}

TEST_CASE("the header's left-hand control folds the folder pane away") {
  EditorAssetBrowserWidget browser = makeBrowser({"props/barrel.obj"});
  const eng::Rect toggle = assetNavToggleRect(browser.layout().header);

  browser.handleMouseDown(centreOf(toggle));

  REQUIRE(browser.navCollapsed());
  REQUIRE(browser.layout().nav.w == Approx(0.0f));
  // The panel keeps its height: only the pane went away.
  REQUIRE(browser.preferredHeight() == Approx(ASSET_PANEL_HEIGHT));
}

TEST_CASE("folding the folder pane widens the grid") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const size_t before = assetCardsPerRow(browser.layout().grid);

  browser.handleMouseDown(
      centreOf(assetNavToggleRect(browser.layout().header)));

  REQUIRE(assetCardsPerRow(browser.layout().grid) > before);
}

TEST_CASE("folding the folder pane keeps the selection") {
  EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "props/barrel.obj"});
  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));
  const size_t selected = browser.selectedFolder();

  browser.hideFolderPane();

  // The pane is how a folder is chosen, not what makes the choice stick.
  REQUIRE(browser.selectedFolder() == selected);
  REQUIRE(browser.visibleAssets().size() == 1);
}

TEST_CASE("a folded panel takes no press below its header") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const eng::Rect card = browser.cardRect(0);
  browser.collapsePanel();

  REQUIRE_FALSE(browser.handleMouseDown(centreOf(card)));
  REQUIRE(browser.draggingIndex() == -1);
  REQUIRE(browser.hitTestCard(card.x + 2.0f, card.y + 2.0f) == -1);
}

TEST_CASE("a folded panel takes no press in the folder pane") {
  EditorAssetBrowserWidget browser = makeBrowser({"props/barrel.obj"});
  const eng::Rect row = browser.folderRowRect(1);
  browser.collapsePanel();

  REQUIRE_FALSE(browser.handleMouseDown(centreOf(row)));
  REQUIRE(browser.hitTestFolderRow(row.x + 2.0f, row.y + 2.0f) == -1);
}

TEST_CASE("a folded panel does not scroll") {
  std::vector<std::string> many;
  for (int i = 0; i < 60; ++i) {
    many.push_back("asset" + std::to_string(i) + ".obj");
  }
  EditorAssetBrowserWidget browser = makeBrowser(many);
  const eng::Rect grid = browser.layout().grid;
  browser.collapsePanel();

  REQUIRE_FALSE(
      browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, -1.0f)));
}

TEST_CASE("folding the panel mid-drag drops the drag") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  bool dropped = false;
  browser.on_asset_dropped = [&](size_t, float, float) {
    dropped = true;
  };
  browser.handleMouseDown(centreOf(browser.cardRect(0)));

  browser.collapsePanel();
  browser.handleMouseUp(mouseAt(600.0f, 300.0f));

  // The grid the drag came out of is gone; placing from it would be a lie.
  REQUIRE(browser.draggingIndex() == -1);
  REQUIRE_FALSE(dropped);
}

TEST_CASE("the nav control does nothing while the panel is folded") {
  EditorAssetBrowserWidget browser = makeBrowser({"props/barrel.obj"});
  browser.collapsePanel();

  browser.handleMouseDown(
      centreOf(assetNavToggleRect(browser.layout().header)));

  REQUIRE_FALSE(browser.navCollapsed());
}

TEST_CASE("unfolding restores the panes as they were") {
  EditorAssetBrowserWidget browser =
      makeBrowser({"crate.obj", "props/barrel.obj"});
  browser.hideFolderPane();
  browser.collapsePanel();

  browser.expandPanel();

  REQUIRE(browser.navCollapsed());
  REQUIRE(browser.layout().grid.w == Approx(browser.rect.w));
}

TEST_CASE("rescanning leaves the fold state alone") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  browser.hideFolderPane();
  browser.collapsePanel();

  // Folding is how the user wants to look at the project, not something
  // the project says, so a rescan has no business undoing it.
  browser.setAssets(EditorAssetTree{}, {});

  REQUIRE(browser.navCollapsed());
  REQUIRE(browser.panelCollapsed());
}
