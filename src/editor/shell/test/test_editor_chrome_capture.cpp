#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <editor/shell/editor-toolbar-widget.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <string>

using namespace eng::editor;

namespace {

constexpr uint32_t CAPTURE_W = 960;
constexpr uint32_t CAPTURE_H = 540;
constexpr float TITLE_H = 28.0f;

/// Renders the editor toolbar through the engine's CPU rasterizer.
///
/// No window and no GPU: GuiRendererContext::init(nullptr) is the engine's
/// unit-test mode, and GuiSoftwareRasterizer turns the emitted vertex buffer
/// into pixels. The widget tests assert rects; this asserts what lands on
/// screen.
///
/// The viewport is deliberately excluded. GuiSoftwareRasterizer approximates
/// every line as its axis-aligned bounding box (see the header's scope
/// limits), and the viewport is drawn almost entirely from lines — its grid
/// and axes rasterize into large rectangles that cover the frame and make
/// pixel assertions meaningless. Verifying the viewport visually needs a
/// real backend capture through the RHI, which is a golden-image job
/// (docs/development/REQUIREMENTS.md §5.1), not this one.
struct ChromeCapture {
  eng::GuiWidgetTree tree;
  eng::GuiRendererContext renderer;
  eng::GuiWidgetId root = eng::GUI_WIDGET_ID_INVALID;
  eng::GuiWidgetId toolbar_id = eng::GUI_WIDGET_ID_INVALID;
  eng::ImageData image;

  ChromeCapture() {
    root = tree.createWidget(eng::GuiWidgetType::PANEL,
                             eng::GUI_WIDGET_ID_INVALID);
    auto* root_panel = dynamic_cast<eng::GuiPanel*>(tree.findWidget(root));
    root_panel->rect = eng::makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                                     static_cast<float>(CAPTURE_H));
    // GuiColor default-constructs opaque, so an unset fill paints black.
    // Give the backdrop the editor's own background token instead.
    root_panel->fill_color = eng::THEME_BG;

    toolbar_id = tree.insertExternalWidget(
        std::make_unique<EditorToolbarWidget>(), root);
    auto* bar = dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id));
    bar->init(tree);
    bar->setProjectName("Transit Station");
    bar->setStatusText("zoom 100%   tile 0, 0");
    bar->setActiveTool(EditorTool::TILE_PAINT);
    bar->layout(tree,
                eng::makeRect(0.0f, TITLE_H, static_cast<float>(CAPTURE_W),
                              TOOLBAR_HEIGHT));
    bar->tick(tree);

    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = CAPTURE_W;
    renderer.viewport_height = CAPTURE_H;

    eng::GuiDrawContext ctx;
    ctx.renderer = &renderer;
    tree.visitDrawOrder([&ctx](const eng::GuiWidget& widget) {
      if (widget.visible) {
        widget.render(ctx);
      }
    });

    image = eng::GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices,
        eng::makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                      static_cast<float>(CAPTURE_H)),
        eng::GUI_RASTER_DEFAULT_BG);
  }

  ~ChromeCapture() { renderer.shutdown(); }
  ChromeCapture(const ChromeCapture&) = delete;
  ChromeCapture& operator=(const ChromeCapture&) = delete;
  ChromeCapture(ChromeCapture&&) = delete;
  ChromeCapture& operator=(ChromeCapture&&) = delete;

  /// Read a pixel as (r, g, b).
  [[nodiscard]] std::array<uint8_t, 3> pixel(uint32_t x, uint32_t y) const {
    const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
    return {image.pixels[offset], image.pixels[offset + 1],
            image.pixels[offset + 2]};
  }
};

/// True when a pixel is within `tolerance` of a theme colour on every
/// channel. The rasterizer composites with alpha, so exact equality is not
/// a safe assertion.
bool matches(const std::array<uint8_t, 3>& pixel, const eng::GuiColor& color,
             int tolerance = 6) {
  const auto near = [tolerance](uint8_t a, uint8_t b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= tolerance;
  };
  return near(pixel[0], color.r) && near(pixel[1], color.g) &&
         near(pixel[2], color.b);
}

}  // namespace

TEST_CASE("the toolbar rasterizes to an image of the expected size") {
  ChromeCapture capture;
  REQUIRE(capture.image.width == CAPTURE_W);
  REQUIRE(capture.image.height == CAPTURE_H);
  REQUIRE(capture.image.pixels.size() ==
          static_cast<size_t>(CAPTURE_W) * CAPTURE_H * 4);
}

TEST_CASE("the toolbar band paints the panel colour") {
  ChromeCapture capture;
  // A point inside the toolbar strip, left of the first button.
  const auto sample = capture.pixel(4, static_cast<uint32_t>(TITLE_H) + 18);
  REQUIRE(matches(sample, eng::THEME_PANEL));
}

TEST_CASE("the active tool button paints the accent colour") {
  ChromeCapture capture;
  // TILE_PAINT is active; find any accent-coloured pixel along the button row.
  const auto row = static_cast<uint32_t>(TITLE_H + TOOLBAR_HEIGHT * 0.5f);
  bool found_accent = false;
  for (uint32_t x = 0; x < CAPTURE_W && !found_accent; ++x) {
    found_accent = matches(capture.pixel(x, row), eng::THEME_ACCENT);
  }
  REQUIRE(found_accent);
}

TEST_CASE("nothing is painted above the toolbar") {
  ChromeCapture capture;
  // The toolbar starts at TITLE_H; the band above it belongs to the title
  // bar, which this capture does not build, so the backdrop shows through.
  for (uint32_t y = 0; y + 1 < static_cast<uint32_t>(TITLE_H); ++y) {
    for (uint32_t x = 0; x < CAPTURE_W; x += 32) {
      REQUIRE(matches(capture.pixel(x, y), eng::THEME_BG));
    }
  }
}

TEST_CASE("the capture can be written to PNG for visual inspection") {
  ChromeCapture capture;
  // Honour CTest's working directory; the file is an artifact, not an
  // assertion, so a write failure is reported rather than asserted on.
  const std::string path = "editor-toolbar-capture.png";
  const bool written =
      eng::GuiSoftwareRasterizer::writePng(capture.image, path);
  INFO("wrote " << path << ": " << written);
  SUCCEED();
}
