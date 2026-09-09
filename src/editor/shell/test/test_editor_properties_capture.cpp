#include "capture-font.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-properties-widget.h>
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

constexpr uint32_t CAPTURE_W = 420;
constexpr uint32_t CAPTURE_H = 300;

/// Renders the properties panel through the engine's CPU rasterizer.
///
/// The widget tests assert rects and reported edits; this asserts what
/// lands on screen — that the rows, the value boxes, and the step buttons
/// actually paint where the layout says they do, and that a panel with
/// nothing selected paints nothing at all.
struct PropertiesCapture {
  eng::GuiWidgetTree tree;
  eng::GuiRendererContext renderer;
  eng::editor::test::CaptureFont font;
  eng::GuiWidgetId root = eng::GUI_WIDGET_ID_INVALID;
  eng::GuiWidgetId panel_id = eng::GUI_WIDGET_ID_INVALID;
  eng::ImageData image;

  PropertiesCapture() {
    buildTree();
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = CAPTURE_W;
    renderer.viewport_height = CAPTURE_H;
    recapture();
  }

  static eng::Rect window() {
    return eng::makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                         static_cast<float>(CAPTURE_H));
  }

  /// The panel, against the right edge as the editor lays it out.
  static eng::Rect panelRect() {
    return eng::makeRect(static_cast<float>(CAPTURE_W) - PROPERTIES_PANEL_WIDTH,
                         0.0f, PROPERTIES_PANEL_WIDTH,
                         static_cast<float>(CAPTURE_H));
  }

  /// A placement with a value in every field, so no row is a row of zeroes.
  static EditorPlacement placement() {
    EditorPlacement out;
    out.position = {12.0f, -3.5f, 1.25f};
    out.rotation = {0.0f, 45.0f, -90.0f};
    return out;
  }

  void buildTree() {
    root = tree.createWidget(eng::GuiWidgetType::PANEL,
                             eng::GUI_WIDGET_ID_INVALID);
    auto* root_panel = dynamic_cast<eng::GuiPanel*>(tree.findWidget(root));
    root_panel->rect = window();
    root_panel->fill_color = eng::THEME_BG;
    panel_id = tree.insertExternalWidget(
        std::make_unique<EditorPropertiesWidget>(), root);
    panelMutable().rect = panelRect();
    panelMutable().setSelection("crate", placement());
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

  /// Re-render after changing what the panel is showing.
  void recapture() {
    renderer.beginFrame();
    renderTree();
    image = eng::GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices, window(), eng::GUI_RASTER_DEFAULT_BG, font.atlas());
  }

  ~PropertiesCapture() { renderer.shutdown(); }
  PropertiesCapture(const PropertiesCapture&) = delete;
  PropertiesCapture& operator=(const PropertiesCapture&) = delete;
  PropertiesCapture(PropertiesCapture&&) = delete;
  PropertiesCapture& operator=(PropertiesCapture&&) = delete;

  /// Read a pixel as (r, g, b).
  [[nodiscard]] std::array<uint8_t, 3> pixel(float x, float y) const {
    const auto px = static_cast<uint32_t>(x);
    const auto py = static_cast<uint32_t>(y);
    const size_t offset = (static_cast<size_t>(py) * image.width + px) * 4;
    return {image.pixels[offset], image.pixels[offset + 1],
            image.pixels[offset + 2]};
  }

  /// The panel, for changing what the next capture shows.
  [[nodiscard]] EditorPropertiesWidget& panelMutable() {
    auto* widget =
        dynamic_cast<EditorPropertiesWidget*>(tree.findWidget(panel_id));
    REQUIRE(widget != nullptr);
    return *widget;
  }

  /// The panel, for asking where it put things.
  [[nodiscard]] const EditorPropertiesWidget& panel() const {
    const auto* widget =
        dynamic_cast<const EditorPropertiesWidget*>(tree.findWidget(panel_id));
    REQUIRE(widget != nullptr);
    return *widget;
  }
};

/// True when a pixel is within `tolerance` of a colour on every channel.
bool matches(const std::array<uint8_t, 3>& pixel, const eng::GuiColor& color,
             int tolerance = 6) {
  const auto close = [tolerance](uint8_t a, uint8_t b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= tolerance;
  };
  return close(pixel[0], color.r) && close(pixel[1], color.g) &&
         close(pixel[2], color.b);
}

}  // namespace

TEST_CASE("the properties panel rasterizes to an image of the right size") {
  const PropertiesCapture capture;
  REQUIRE(capture.image.width == CAPTURE_W);
  REQUIRE(capture.image.height == CAPTURE_H);
}

TEST_CASE("the panel leaves the viewport beside it alone") {
  const PropertiesCapture capture;
  // The viewport is to the left, and the panel taking its width is a layout
  // decision — painting into it would be a bug.
  const auto beside =
      capture.pixel(PropertiesCapture::panelRect().x - 8.0f, CAPTURE_H / 2);
  REQUIRE(matches(beside, eng::THEME_BG));
}

TEST_CASE("the panel body paints its own fill") {
  const PropertiesCapture capture;
  const eng::Rect body = capture.panel().layout().body;
  // Below the last row, where nothing else is drawn.
  const auto sample = capture.pixel(body.x + 4.0f, body.y + body.h - 8.0f);
  REQUIRE(matches(sample, eng::THEME_PANEL));
}

TEST_CASE("the header strip is darker than the body under it") {
  const PropertiesCapture capture;
  const EditorPropertiesLayout layout = capture.panel().layout();
  const auto header = capture.pixel(layout.header.x + layout.header.w - 6.0f,
                                    layout.header.y + layout.header.h * 0.5f);
  REQUIRE(matches(header, eng::THEME_BG));
}

TEST_CASE("a value box paints against the panel behind it") {
  const PropertiesCapture capture;
  const eng::Rect row =
      capture.panel().fieldRowRect(EditorPropertyField::POSITION_X);
  const eng::Rect value = propertyValueRect(row);
  // Sample the well's left edge, clear of the centred digits.
  const auto sample = capture.pixel(value.x + 3.0f, value.y + value.h * 0.5f);
  REQUIRE_FALSE(matches(sample, eng::THEME_PANEL));
}

TEST_CASE("both step buttons paint on every row") {
  const PropertiesCapture capture;
  for (EditorPropertyField field : EDITOR_PLACEMENT_FIELDS) {
    const eng::Rect row = capture.panel().fieldRowRect(field);
    const eng::Rect decrement = propertyDecrementRect(row);
    const eng::Rect increment = propertyIncrementRect(row);
    // The middle of a button is its sign, so sample just inside its edge.
    const auto left =
        capture.pixel(decrement.x + decrement.w * 0.5f, decrement.y + 3.0f);
    const auto right =
        capture.pixel(increment.x + increment.w * 0.5f, increment.y + 3.0f);
    REQUIRE_FALSE(matches(left, eng::THEME_PANEL));
    REQUIRE_FALSE(matches(right, eng::THEME_PANEL));
  }
}

TEST_CASE("a panel with nothing selected paints nothing at all") {
  PropertiesCapture capture;
  capture.panelMutable().clearSelection();
  capture.recapture();
  // The backdrop shows through where the panel was: an empty panel that
  // still painted would take a strip of viewport for nothing.
  const eng::Rect panel = PropertiesCapture::panelRect();
  REQUIRE(matches(capture.pixel(panel.x + panel.w * 0.5f, panel.h * 0.5f),
                  eng::THEME_BG));
}

TEST_CASE("the properties capture can be written to PNG for inspection") {
  const PropertiesCapture capture;
  // Honour CTest's working directory; the file is an artifact, not an
  // assertion, so a write failure is reported rather than asserted on.
  const std::string path = "editor-properties-capture.png";
  const bool written =
      eng::GuiSoftwareRasterizer::writePng(capture.image, path);
  INFO("wrote " << path << ": " << written);
  SUCCEED();
}

TEST_CASE("a light's panel lists its own rows, not a placement's") {
  PropertiesCapture capture;
  EditorLight light =
      makeEditorLight(EditorLightKind::POINT, {12.0f, -3.5f, 3.0f});
  light.color = {1.0f, 0.85f, 0.6f};
  capture.panelMutable().setSelection("Point Light", light);
  capture.recapture();

  // The range row is where a placement's fourth row — a rotation — would
  // be, so a panel showing one is unmistakably showing a light.
  const eng::Rect range =
      capture.panel().fieldRowRect(EditorPropertyField::RANGE);
  REQUIRE(range.w > 0.0f);
  REQUIRE(capture.panel().fieldRowRect(EditorPropertyField::ROTATION_X).w ==
          0.0f);
  REQUIRE_FALSE(
      matches(capture.pixel(range.x + range.w * 0.5f, range.y + range.h * 0.5f),
              eng::THEME_PANEL));
}

TEST_CASE("the light properties capture can be written for inspection") {
  PropertiesCapture capture;
  capture.panelMutable().setSelection(
      "Point Light",
      makeEditorLight(EditorLightKind::POINT, {12.0f, -3.5f, 3.0f}));
  capture.recapture();

  const std::string path = "editor-properties-light-capture.png";
  const bool written =
      eng::GuiSoftwareRasterizer::writePng(capture.image, path);
  INFO("wrote " << path << ": " << written);
  SUCCEED();
}
