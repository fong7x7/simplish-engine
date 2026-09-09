#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-asset-browser-widget.h>
#include <editor/shell/editor-general-section.h>
#include <editor/shell/editor-shape.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
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

/// Point an existing browser at a project holding the given relative asset
/// paths, the way a rescan does.
void loadInto(EditorAssetBrowserWidget& browser,
              const std::vector<std::string>& paths) {
  EditorAssetScan scan;
  for (const std::string& path : paths) {
    scan.assets.push_back(makeAsset(path));
  }
  std::vector<std::string> names;
  for (const EditorAsset& asset : scan.assets) {
    names.push_back(asset.name);
  }
  browser.setAssets(buildEditorAssetTree(scan), std::move(names));
}

/// A browser listing the given relative asset paths.
EditorAssetBrowserWidget makeBrowser(const std::vector<std::string>& paths) {
  EditorAssetBrowserWidget browser;
  browser.rect = PANEL_RECT;
  loadInto(browser, paths);
  return browser;
}

/// The names the editor hands the browser for a list of assets — the
/// shapes among them — with the light items numbered after every one.
std::vector<std::string> namesWithGeneral(const std::vector<EditorAsset>& all) {
  std::vector<std::string> names;
  for (const EditorAsset& asset : all) {
    names.push_back(asset.name);
  }
  for (const EditorGeneralItem item : EDITOR_GENERAL_ITEMS) {
    names.emplace_back(editorGeneralItemName(item));
  }
  return names;
}

/// A browser listing those assets and the built-in general section above
/// them, built exactly as the editor builds it.
EditorAssetBrowserWidget
makeBrowserWithGeneral(const std::vector<std::string>& paths) {
  EditorAssetScan scan;
  for (const std::string& path : paths) {
    scan.assets.push_back(makeAsset(path));
  }
  EditorAssetTree tree = buildEditorAssetTree(scan);
  std::vector<EditorAsset> all = scan.assets;
  const size_t first_shape = appendEditorShapeAssets(all);
  appendEditorGeneralSection(tree, first_shape, all.size());
  EditorAssetBrowserWidget browser;
  browser.rect = PANEL_RECT;
  browser.setAssets(std::move(tree), namesWithGeneral(all));
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

/// A browser listing enough assets that its grid has to scroll.
EditorAssetBrowserWidget makeScrollingBrowser() {
  std::vector<std::string> many;
  for (int i = 0; i < 60; ++i) {
    many.push_back("asset" + std::to_string(i) + ".obj");
  }
  return makeBrowser(many);
}

/// Render the browser through a device-less renderer and hand back the
/// command stream, which is where clipping actually lives.
struct RenderedBrowser {
  eng::GuiRendererContext renderer;

  explicit RenderedBrowser(const EditorAssetBrowserWidget& browser) {
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = 1280;
    renderer.viewport_height = 900;
    renderer.beginFrame();
    eng::GuiDrawContext ctx;
    ctx.renderer = &renderer;
    browser.render(ctx);
  }
  ~RenderedBrowser() { renderer.shutdown(); }
  RenderedBrowser(const RenderedBrowser&) = delete;
  RenderedBrowser& operator=(const RenderedBrowser&) = delete;
  RenderedBrowser(RenderedBrowser&&) = delete;
  RenderedBrowser& operator=(RenderedBrowser&&) = delete;

  /// Whether a scissor covering exactly @p clip was pushed.
  [[nodiscard]] bool pushed(const eng::Rect& clip) const {
    for (const auto& cmd : renderer.commands) {
      if (cmd.type != eng::DrawCommandType::PUSH_SCISSOR) {
        continue;
      }
      if (cmd.scissor.x == Approx(clip.x) && cmd.scissor.y == Approx(clip.y) &&
          cmd.scissor.w == Approx(clip.w) && cmd.scissor.h == Approx(clip.h)) {
        return true;
      }
    }
    return false;
  }
};

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

TEST_CASE("a scrolled card would otherwise reach outside the panel") {
  EditorAssetBrowserWidget browser = makeScrollingBrowser();
  const eng::Rect grid = browser.layout().grid;

  for (int i = 0; i < 20; ++i) {
    browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, -1.0f));
  }

  // Not a hypothetical: three rows of cards in a 198px grid scroll far
  // enough that the top row clears the panel's own top edge. This is what
  // the clip below is for, and without the clip it paints over the
  // viewport.
  REQUIRE(browser.cardRect(0).y < browser.rect.y);
}

TEST_CASE("the cards are clipped to the grid") {
  const EditorAssetBrowserWidget browser = makeScrollingBrowser();
  const RenderedBrowser rendered(browser);

  REQUIRE(rendered.pushed(browser.layout().grid));
}

TEST_CASE("the folder rows are clipped to the pane") {
  const EditorAssetBrowserWidget browser =
      makeBrowser({"props/barrel.obj", "terrain/mud.obj"});
  const RenderedBrowser rendered(browser);

  REQUIRE(rendered.pushed(browser.layout().nav));
}

TEST_CASE("every clip the browser pushes is popped again") {
  EditorAssetBrowserWidget browser = makeScrollingBrowser();
  browser.handleMouseDown(centreOf(browser.cardRect(0)));
  browser.handleMouseMove(mouseAt(400.0f, 200.0f));
  const RenderedBrowser rendered(browser);

  // A push left open runs on into whatever is drawn next, and a stray pop
  // unclips something that was meant to stay clipped.
  int depth = 0;
  for (const auto& cmd : rendered.renderer.commands) {
    depth += cmd.type == eng::DrawCommandType::PUSH_SCISSOR ? 1 : 0;
    depth -= cmd.type == eng::DrawCommandType::POP_SCISSOR ? 1 : 0;
    REQUIRE(depth >= 0);
  }
  REQUIRE(depth == 0);
}

TEST_CASE("a folded panel pushes no clip at all") {
  EditorAssetBrowserWidget browser = makeScrollingBrowser();
  browser.collapsePanel();
  const RenderedBrowser rendered(browser);

  size_t pushes = 0;
  for (const auto& cmd : rendered.renderer.commands) {
    pushes += cmd.type == eng::DrawCommandType::PUSH_SCISSOR ? 1 : 0;
  }
  REQUIRE(pushes == 0);
}

TEST_CASE("the drag ghost is not clipped to either pane") {
  EditorAssetBrowserWidget browser = makeScrollingBrowser();
  browser.handleMouseDown(centreOf(browser.cardRect(0)));
  browser.handleMouseMove(mouseAt(400.0f, 200.0f));
  const RenderedBrowser rendered(browser);

  // The ghost follows the cursor over the viewport, so it has to be drawn
  // after every clip has been popped.
  const auto& commands = rendered.renderer.commands;
  size_t last_pop = 0;
  for (size_t i = 0; i < commands.size(); ++i) {
    if (commands[i].type == eng::DrawCommandType::POP_SCISSOR) {
      last_pop = i;
    }
  }
  REQUIRE(last_pop + 1 < commands.size());
}

TEST_CASE("rescanning does not open folders from the tree before it") {
  EditorAssetBrowserWidget browser = makeBrowser({"terrain/rocks/boulder.obj"});
  // Open the one folder that can be opened, which is index 1 in this tree.
  browser.expandFolder(browser.folderRows()[1].folder);
  REQUIRE(browser.folderRows().size() == 3);

  // The same browser, pointed at a project whose index 1 is a different
  // folder that also has children — which is what opening another project
  // does to it.
  loadInto(browser, {"audio/music/theme.obj", "props/crate.obj"});

  // Expansion is remembered as folder indices, so carrying the set over
  // would open whichever folder landed on the old number: "audio" here,
  // which nobody touched. Four rows instead of three is that bug.
  REQUIRE(browser.folderRows().size() == 3);
  REQUIRE(browser.folderExpanded(EDITOR_ASSET_FOLDER_ROOT));
  for (const EditorAssetFolderRow& row : browser.folderRows()) {
    if (row.folder != EDITOR_ASSET_FOLDER_ROOT) {
      REQUIRE_FALSE(row.expanded);
    }
  }
}

TEST_CASE("a card with no picture yet still draws its well") {
  const EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  const RenderedBrowser rendered(browser);

  // Cards keep their shape whether or not a picture has arrived; the only
  // thing that changes is what fills the well.
  REQUIRE_FALSE(rendered.renderer.vertices.empty());
}

TEST_CASE("a texture handed to the browser reaches the card that draws it") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj", "barrel.obj"});
  browser.setAssetThumbnail(1, 42);
  const RenderedBrowser rendered(browser);

  bool textured = false;
  for (const auto& cmd : rendered.renderer.commands) {
    textured = textured || cmd.batch.texture == 42;
  }
  REQUIRE(textured);
}

TEST_CASE("a texture for an asset that does not exist is ignored") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});

  // The editor indexes by the whole asset list, and a rescan can shorten it
  // under an in-flight upload.
  browser.setAssetThumbnail(99, 42);

  const RenderedBrowser rendered(browser);
  for (const auto& cmd : rendered.renderer.commands) {
    REQUIRE(cmd.batch.texture != 42);
  }
}

TEST_CASE("rescanning forgets the textures the last project had") {
  EditorAssetBrowserWidget browser = makeBrowser({"crate.obj"});
  browser.setAssetThumbnail(0, 42);

  // The editor destroys those textures when it replaces the list, so a
  // handle kept here would name a texture that no longer exists.
  loadInto(browser, {"other.obj"});

  const RenderedBrowser rendered(browser);
  for (const auto& cmd : rendered.renderer.commands) {
    REQUIRE(cmd.batch.texture != 42);
  }
}

TEST_CASE("the browser names only the cards on screen as visible") {
  std::vector<std::string> many;
  for (int i = 0; i < 60; ++i) {
    many.push_back("asset" + std::to_string(i) + ".obj");
  }
  const EditorAssetBrowserWidget browser = makeBrowser(many);

  // Every asset is listed, but only a screenful is worth making pictures
  // for — that gap is the whole point of generating them lazily.
  REQUIRE(browser.visibleSlotCount() > 0);
  REQUIRE(browser.visibleSlotCount() < browser.assetCount());
}

TEST_CASE("scrolling moves which cards the browser names as visible") {
  EditorAssetBrowserWidget browser = makeScrollingBrowser();
  const eng::Rect grid = browser.layout().grid;
  const size_t before = browser.firstVisibleSlot();

  for (int i = 0; i < 12; ++i) {
    browser.handleScroll(scrollAt(grid.x + 20.0f, grid.y + 20.0f, -1.0f));
  }

  REQUIRE(browser.firstVisibleSlot() > before);
}

TEST_CASE("a folder with nothing in it names no visible cards") {
  EditorAssetBrowserWidget browser = makeBrowser({"terrain/rocks/boulder.obj"});
  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));

  REQUIRE(browser.visibleAssets().empty());
  REQUIRE(browser.visibleSlotCount() == 0);
}

TEST_CASE("the general section is the first row, above the assets") {
  const EditorAssetBrowserWidget browser =
      makeBrowserWithGeneral({"crate.obj"});
  const auto& rows = browser.folderRows();

  // The section opens on its own, so its two subsections are listed under
  // it and the assets root follows them.
  REQUIRE(rows.size() == 4);
  REQUIRE(rows[0].depth == 0);
  REQUIRE(rows[1].depth == 1);
  REQUIRE(rows[2].depth == 1);
  REQUIRE(rows[3].folder == EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(rows[3].depth == 0);
}

TEST_CASE("the browser opens on the assets, not on the built-in section") {
  const EditorAssetBrowserWidget browser =
      makeBrowserWithGeneral({"crate.obj"});
  // The section is listed first, but a project's own assets are what
  // somebody opening the browser came for.
  REQUIRE(browser.selectedFolder() == EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(browser.visibleAssets() == std::vector<size_t>{0});
}

TEST_CASE("selecting the lighting subsection shows the lights as cards") {
  EditorAssetBrowserWidget browser = makeBrowserWithGeneral({"crate.obj"});
  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));

  REQUIRE(browser.visibleAssets().size() == EDITOR_GENERAL_ITEM_COUNT);
  // Past the one asset and the shapes among them, which is how the editor
  // tells a light from a model when the card is dropped.
  REQUIRE(browser.visibleAssets().front() == 1 + EDITOR_SHAPE_COUNT);
}

TEST_CASE("selecting the shapes subsection shows the shapes as cards") {
  EditorAssetBrowserWidget browser = makeBrowserWithGeneral({"crate.obj"});
  browser.handleMouseDown(centreOf(browser.folderRowRect(2)));

  REQUIRE(browser.visibleAssets().size() == EDITOR_SHAPE_COUNT);
  // A shape is an asset, numbered straight after the scanned one.
  REQUIRE(browser.visibleAssets().front() == 1);
}

TEST_CASE("the general section itself holds no cards") {
  EditorAssetBrowserWidget browser = makeBrowserWithGeneral({"crate.obj"});
  browser.handleMouseDown(centreOf(browser.folderRowRect(0)));

  REQUIRE(browser.visibleAssets().empty());
}

TEST_CASE("dragging a light out reports its own entry number") {
  EditorAssetBrowserWidget browser =
      makeBrowserWithGeneral({"crate.obj", "keg.obj"});
  size_t dropped = 0;
  browser.on_asset_dropped = [&](size_t entry, float, float) {
    dropped = entry;
  };
  browser.handleMouseDown(centreOf(browser.folderRowRect(1)));

  browser.handleMouseDown(centreOf(browser.cardRect(1)));
  browser.handleMouseUp(mouseAt(600.0f, 300.0f));

  // Two assets and the shapes after them, so the second light is the entry
  // one past the first.
  REQUIRE(dropped == 2 + EDITOR_SHAPE_COUNT + 1);
}

TEST_CASE("dragging a shape out reports the asset number it took") {
  EditorAssetBrowserWidget browser =
      makeBrowserWithGeneral({"crate.obj", "keg.obj"});
  size_t dropped = 0;
  browser.on_asset_dropped = [&](size_t entry, float, float) {
    dropped = entry;
  };
  browser.handleMouseDown(centreOf(browser.folderRowRect(2)));

  browser.handleMouseDown(centreOf(browser.cardRect(0)));
  browser.handleMouseUp(mouseAt(600.0f, 300.0f));

  // The first shape sits straight after the two scanned assets, and the
  // editor places it exactly as it places one of them.
  REQUIRE(dropped == 2);
}
