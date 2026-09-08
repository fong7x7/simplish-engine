#include "capture-font.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <editor/shell/editor-asset-browser-widget.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-widget-tree.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

constexpr uint32_t CAPTURE_W = 960;
constexpr uint32_t CAPTURE_H = 280;

/// Renders the asset browser through the engine's CPU rasterizer.
///
/// The widget tests assert rects and hit tests; this asserts what lands on
/// screen — that the folder pane and the grid actually paint where the
/// layout says they should, and that the two do not overwrite each other.
///
/// Text is real: the fixture loads the machine's UI font, so the folder
/// names and card labels are rasterized the way the editor draws them.
struct BrowserCapture {
  eng::GuiWidgetTree tree;
  eng::GuiRendererContext renderer;
  eng::editor::test::CaptureFont font;
  eng::GuiWidgetId root = eng::GUI_WIDGET_ID_INVALID;
  eng::GuiWidgetId browser_id = eng::GUI_WIDGET_ID_INVALID;
  eng::ImageData image;

  BrowserCapture() {
    buildTree();
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = CAPTURE_W;
    renderer.viewport_height = CAPTURE_H;
    renderTree();
    image = eng::GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices, window(), eng::GUI_RASTER_DEFAULT_BG, font.atlas());
  }

  static eng::Rect window() {
    return eng::makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                         static_cast<float>(CAPTURE_H));
  }

  /// The panel, filling the capture below a strip of backdrop.
  static eng::Rect panelRect() {
    return eng::makeRect(0.0f,
                         static_cast<float>(CAPTURE_H) - ASSET_PANEL_HEIGHT,
                         static_cast<float>(CAPTURE_W), ASSET_PANEL_HEIGHT);
  }

  /// An asset at a relative path, as the scan would have produced it.
  static EditorAsset makeAsset(const std::string& relative) {
    const fs::path path(relative);
    return {path.stem().string(), fs::path("/project/assets") / path, path};
  }

  void buildTree() {
    root = tree.createWidget(eng::GuiWidgetType::PANEL,
                             eng::GUI_WIDGET_ID_INVALID);
    auto* root_panel = dynamic_cast<eng::GuiPanel*>(tree.findWidget(root));
    root_panel->rect = window();
    root_panel->fill_color = eng::THEME_BG;
    browser_id = tree.insertExternalWidget(
        std::make_unique<EditorAssetBrowserWidget>(), root);
    auto* browser =
        dynamic_cast<EditorAssetBrowserWidget*>(tree.findWidget(browser_id));
    browser->rect = panelRect();
    browser->setAssets(buildEditorAssetTree(scan()), names());
    expandAll(*browser);
  }

  /// Open every folder that has children, so the capture shows the indent
  /// that nesting produces.
  static void expandAll(EditorAssetBrowserWidget& browser) {
    // Copied first: expanding rebuilds the rows being walked.
    const std::vector<EditorAssetFolderRow> rows = browser.folderRows();
    for (const EditorAssetFolderRow& row : rows) {
      if (row.has_children) {
        browser.expandFolder(row.folder);
      }
    }
  }

  /// A project with assets at the root and filed under two folders.
  static EditorAssetScan scan() {
    EditorAssetScan out;
    for (const std::string& path :
         {"crate.obj", "ground_tile.obj", "props/barrel.obj",
          "props/lantern.obj", "terrain/rocks/boulder.obj"}) {
      out.assets.push_back(makeAsset(path));
    }
    return out;
  }

  static std::vector<std::string> names() {
    std::vector<std::string> out;
    for (const EditorAsset& asset : scan().assets) {
      out.push_back(asset.name);
    }
    return out;
  }

  void renderTree() {
    eng::GuiDrawContext ctx;
    ctx.renderer = &renderer;
    ctx.text_pipeline = &font.pipeline;
    ctx.face_id = font.face_id;
    tree.visitDrawOrder([&ctx](const eng::GuiWidget& widget) {
      if (widget.visible) {
        widget.render(ctx);
      }
    });
  }

  ~BrowserCapture() { renderer.shutdown(); }
  BrowserCapture(const BrowserCapture&) = delete;
  BrowserCapture& operator=(const BrowserCapture&) = delete;
  BrowserCapture(BrowserCapture&&) = delete;
  BrowserCapture& operator=(BrowserCapture&&) = delete;

  /// Read a pixel as (r, g, b).
  [[nodiscard]] std::array<uint8_t, 3> pixel(uint32_t x, uint32_t y) const {
    const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
    return {image.pixels[offset], image.pixels[offset + 1],
            image.pixels[offset + 2]};
  }

  /// Resize the panel to the height it asks for and re-anchor it to the
  /// bottom, which is what the editor's own layout pass does for it.
  void applyPreferredHeight() {
    EditorAssetBrowserWidget& browser = browserMutable();
    const float height = browser.preferredHeight();
    browser.rect = eng::makeRect(0.0f, static_cast<float>(CAPTURE_H) - height,
                                 static_cast<float>(CAPTURE_W), height);
  }

  /// Re-render after changing what the browser is showing.
  void recapture() {
    renderer.beginFrame();
    renderTree();
    image = eng::GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices, window(), eng::GUI_RASTER_DEFAULT_BG, font.atlas());
  }

  /// The browser, for changing what the next capture shows.
  [[nodiscard]] EditorAssetBrowserWidget& browserMutable() {
    auto* widget =
        dynamic_cast<EditorAssetBrowserWidget*>(tree.findWidget(browser_id));
    REQUIRE(widget != nullptr);
    return *widget;
  }

  /// The browser, for asking where it put things.
  [[nodiscard]] const EditorAssetBrowserWidget& browser() const {
    const auto* widget = dynamic_cast<const EditorAssetBrowserWidget*>(
        tree.findWidget(browser_id));
    REQUIRE(widget != nullptr);
    return *widget;
  }
};

/// True when a pixel is within `tolerance` of a colour on every channel.
/// The rasterizer composites with alpha, so exact equality is not a safe
/// assertion.
bool matches(const std::array<uint8_t, 3>& pixel, const eng::GuiColor& color,
             int tolerance = 6) {
  const auto near = [tolerance](uint8_t a, uint8_t b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= tolerance;
  };
  return near(pixel[0], color.r) && near(pixel[1], color.g) &&
         near(pixel[2], color.b);
}

/// True when two sampled pixels are within `tolerance` on every channel.
bool similar(const std::array<uint8_t, 3>& a, const std::array<uint8_t, 3>& b,
             int tolerance = 2) {
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])) > tolerance) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("the browser rasterizes to an image of the expected size") {
  const BrowserCapture capture;
  REQUIRE(capture.image.width == CAPTURE_W);
  REQUIRE(capture.image.height == CAPTURE_H);
}

TEST_CASE("the backdrop above the panel is left alone") {
  const BrowserCapture capture;
  // The panel must not paint over the viewport that sits above it.
  const auto above = capture.pixel(
      CAPTURE_W / 2, static_cast<uint32_t>(BrowserCapture::panelRect().y) - 6);
  REQUIRE(matches(above, eng::THEME_BG));
}

TEST_CASE("the folder pane paints darker than the panel around it") {
  const BrowserCapture capture;
  const eng::Rect nav = capture.browser().layout().nav;
  const auto inside =
      capture.pixel(static_cast<uint32_t>(nav.x + nav.w - 4.0f),
                    static_cast<uint32_t>(nav.y + nav.h - 4.0f));
  const auto header = capture.pixel(static_cast<uint32_t>(nav.x + nav.w - 4.0f),
                                    static_cast<uint32_t>(nav.y - 6.0f));
  // The pane is distinguishable from the header strip above it, which is
  // the whole reason it has a fill of its own.
  REQUIRE_FALSE(matches(inside, eng::THEME_PANEL));
  REQUIRE(matches(header, eng::THEME_PANEL));
}

TEST_CASE("a card paints inside the grid, clear of the folder pane") {
  const BrowserCapture capture;
  const eng::Rect card = capture.browser().cardRect(0);
  const eng::Rect nav = capture.browser().layout().nav;
  REQUIRE(card.x > nav.x + nav.w);

  // Sample below the card's label so the glyphs do not decide the answer.
  const auto sample =
      capture.pixel(static_cast<uint32_t>(card.x + 6.0f),
                    static_cast<uint32_t>(card.y + card.h - 6.0f));
  REQUIRE_FALSE(matches(sample, eng::THEME_PANEL));
}

TEST_CASE("the selected folder's row is highlighted") {
  const BrowserCapture capture;
  const eng::Rect row = capture.browser().folderRowRect(0);
  const auto sample =
      capture.pixel(static_cast<uint32_t>(row.x + row.w - 6.0f),
                    static_cast<uint32_t>(row.y + row.h * 0.5f));
  const eng::Rect unselected = capture.browser().folderRowRect(1);
  const auto plain =
      capture.pixel(static_cast<uint32_t>(unselected.x + unselected.w - 6.0f),
                    static_cast<uint32_t>(unselected.y + unselected.h * 0.5f));
  REQUIRE_FALSE(similar(sample, plain));
}

TEST_CASE("the browser capture can be written to PNG for inspection") {
  const BrowserCapture capture;
  // Honour CTest's working directory; the file is an artifact, not an
  // assertion, so a write failure is reported rather than asserted on.
  const std::string path = "editor-asset-browser-capture.png";
  const bool written =
      eng::GuiSoftwareRasterizer::writePng(capture.image, path);
  INFO("wrote " << path << ": " << written);
  SUCCEED();
}

TEST_CASE("the folded captures can be written to PNG for inspection") {
  BrowserCapture capture;

  capture.browserMutable().hideFolderPane();
  capture.recapture();
  INFO("wrote nav-folded: " << eng::GuiSoftwareRasterizer::writePng(
           capture.image, "editor-asset-browser-nav-folded-capture.png"));

  capture.browserMutable().showFolderPane();
  capture.browserMutable().collapsePanel();
  capture.applyPreferredHeight();
  capture.recapture();
  INFO("wrote panel-folded: " << eng::GuiSoftwareRasterizer::writePng(
           capture.image, "editor-asset-browser-panel-folded-capture.png"));
  SUCCEED();
}

TEST_CASE("the capture with card pictures can be written for inspection") {
  BrowserCapture capture;
  // The GUI's software rasterizer samples every textured quad from the
  // glyph atlas, so a real thumbnail texture cannot be drawn here — the
  // card wells stand in for it. What this shows is the layout the pictures
  // land in, which is the part worth looking at before they do.
  capture.recapture();
  const bool written = eng::GuiSoftwareRasterizer::writePng(
      capture.image, "editor-asset-browser-cards-capture.png");
  INFO("wrote cards: " << written);
  SUCCEED();
}
