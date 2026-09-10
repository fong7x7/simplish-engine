#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-viewport-widget.h>
#include <editor/shell/iso-projection.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <string>

using namespace eng::editor;

namespace {

/// Whether the first of the four starts is drawn selected.
enum class FirstStart : uint8_t { PLAIN, SELECTED };

constexpr uint32_t CAPTURE_W = 640;
constexpr uint32_t CAPTURE_H = 400;

/// Renders the viewport's overlay — grid, footprints, player start columns
/// — through the engine's CPU rasterizer.
///
/// The widget tests assert picks and camera moves; this asserts that what
/// the editor hands the viewport actually lands on screen in the colour it
/// is meant to, which for a player start is the whole of how anyone sees
/// one.
struct ViewportCapture {
  EditorViewportWidget viewport;
  eng::GuiRendererContext renderer;
  eng::ImageData image;

  ViewportCapture() {
    viewport.rect = window();
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = CAPTURE_W;
    renderer.viewport_height = CAPTURE_H;
  }

  static eng::Rect window() {
    return eng::makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                         static_cast<float>(CAPTURE_H));
  }

  /// A start for each of the four players, a tile apart, the first
  /// selected when @p first says so.
  void placeFourStarts(FirstStart first) {
    for (uint8_t player = 1; player <= EDITOR_PLAYER_SLOTS; ++player) {
      const EditorPlayerStart start = makeEditorPlayerStart(
          player, {static_cast<float>(player) * 1.5f - 3.5f, 0.5f, 0.0f});
      viewport.placement_markers.push_back(
          {editorPlayerStartBounds(start),
           first == FirstStart::SELECTED && player == 1,
           EditorMarkerStyle::PLAYER_START, player});
    }
  }

  void capture() {
    renderer.beginFrame();
    eng::GuiDrawContext ctx;
    ctx.renderer = &renderer;
    viewport.render(ctx);
    image = eng::GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices, window(), eng::GUI_RASTER_DEFAULT_BG);
  }

  /// How many pixels are within a few steps of @p color on every channel.
  [[nodiscard]] size_t countNear(const eng::GuiColor& color) const {
    size_t count = 0;
    for (size_t i = 0; i + 3 < image.pixels.size(); i += 4) {
      count += near(image.pixels[i], color.r) &&
                       near(image.pixels[i + 1], color.g) &&
                       near(image.pixels[i + 2], color.b)
                   ? 1
                   : 0;
    }
    return count;
  }

  static bool near(uint8_t a, uint8_t b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= 12;
  }

  ~ViewportCapture() { renderer.shutdown(); }
  ViewportCapture(const ViewportCapture&) = delete;
  ViewportCapture& operator=(const ViewportCapture&) = delete;
  ViewportCapture(ViewportCapture&&) = delete;
  ViewportCapture& operator=(ViewportCapture&&) = delete;
};

}  // namespace

TEST_CASE("each player's start is drawn in that player's colour") {
  ViewportCapture fixture;
  fixture.placeFourStarts(FirstStart::PLAIN);
  fixture.capture();

  for (const eng::GuiColor& color : EDITOR_PLAYER_START_COLORS) {
    REQUIRE(fixture.countNear(color) > 20);
  }
}

TEST_CASE("a selected start is drawn in the selection colour, not its own") {
  ViewportCapture fixture;
  fixture.placeFourStarts(FirstStart::SELECTED);
  fixture.capture();

  // Player 1's green is gone; the other three keep theirs.
  REQUIRE(fixture.countNear(EDITOR_PLAYER_START_COLORS[0]) == 0);
  REQUIRE(fixture.countNear(EDITOR_PLAYER_START_COLORS[1]) > 20);
}

TEST_CASE("the viewport capture can be written to PNG for inspection") {
  ViewportCapture fixture;
  fixture.viewport.camera.zoom = 2.0f;
  fixture.placeFourStarts(FirstStart::SELECTED);
  fixture.viewport.placement_markers.push_back(
      {{{-1.0f, 2.0f, 0.0f}, {0.0f, 3.0f, 1.0f}}, false});
  fixture.capture();

  const std::string path = "editor-viewport-capture.png";
  REQUIRE(eng::GuiSoftwareRasterizer::writePng(fixture.image, path));
}
