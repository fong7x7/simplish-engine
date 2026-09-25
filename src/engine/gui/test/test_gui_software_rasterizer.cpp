#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/gui-vertex.h>

// The rasterizer shades shapes with the GUI shader's own maths
// (`gui-quad-shading.cpp`), so these pin that maths down too.

using namespace eng;

namespace {

/// Opaque black: the capture background.
constexpr uint32_t BLACK = 0xFF000000u;
constexpr uint32_t WHITE = 0xFFFFFFFFu;
constexpr uint32_t RED = 0xFF0000FFu;
constexpr uint32_t BLUE = 0xFFFF0000u;

/// The 100 × 60 capture of one shape filling it, drawn as @p style says.
ImageData capture(const GuiVertex& style) {
  GuiRendererContext renderer;
  renderer.beginFrame();
  renderer.emitShape({0.0f, 0.0f, 100.0f, 60.0f}, style);
  return GuiSoftwareRasterizer::rasterizeQuads(
      renderer.vertices, {0.0f, 0.0f, 100.0f, 60.0f}, BLACK);
}

/// Channel @p c (0 red … 2 blue) of the pixel at (x, y).
int channel(const ImageData& image, uint32_t x, uint32_t y, size_t c) {
  return image.pixels[(static_cast<size_t>(y) * image.width + x) * 4 + c];
}

/// The red channel at (x, y): how much of the white shape shows there.
int shown(const ImageData& image, uint32_t x, uint32_t y) {
  return channel(image, x, y, 0);
}

}  // namespace

TEST_CASE("a rounded shape covers its middle and not its corners") {
  GuiVertex style{.color = WHITE, .color2 = WHITE, .flags = GUI_VERTEX_SHAPE};
  std::fill(std::begin(style.radii), std::end(style.radii), 20.0f);
  const ImageData image = capture(style);
  CHECK(shown(image, 50, 30) == 255);
  CHECK(shown(image, 0, 0) == 0);
  CHECK(shown(image, 99, 59) == 0);
  CHECK(shown(image, 50, 0) > 100);
}

TEST_CASE("each corner takes its own radius") {
  GuiVertex style{.color = WHITE, .color2 = WHITE, .flags = GUI_VERTEX_SHAPE};
  style.radii[0] = 25.0f;
  const ImageData image = capture(style);
  CHECK(shown(image, 0, 0) == 0);
  CHECK(shown(image, 99, 0) > 200);
  CHECK(shown(image, 0, 59) > 200);
}

TEST_CASE("a border covers only the ring its sides give it") {
  GuiVertex style{.color = WHITE, .color2 = WHITE};
  style.border[0] = 4.0f;
  style.border[3] = 12.0f;
  const ImageData image = capture(style);
  CHECK(shown(image, 50, 1) > 200);
  CHECK(shown(image, 6, 30) > 200);
  CHECK(shown(image, 50, 30) == 0);
  CHECK(shown(image, 98, 30) == 0);
}

TEST_CASE("a linear gradient runs its angle; a radial one runs outwards") {
  GuiVertex style{.color = RED,
                  .color2 = BLUE,
                  .flags = GUI_VERTEX_LINEAR_GRADIENT,
                  .param = 0.0f};
  const ImageData linear = capture(style);
  CHECK(channel(linear, 0, 30, 0) > 240);
  CHECK(channel(linear, 99, 30, 2) > 240);
  CHECK(channel(linear, 50, 30, 0) > 100);
  CHECK(channel(linear, 50, 30, 2) > 100);
  style.flags = GUI_VERTEX_RADIAL_GRADIENT;
  const ImageData radial = capture(style);
  CHECK(channel(radial, 50, 30, 0) > 240);
  CHECK(channel(radial, 99, 30, 2) > 240);
}

TEST_CASE("a shadow is solid inside its shape and fades beyond it") {
  const GuiVertex style{.color = WHITE,
                        .color2 = WHITE,
                        .flags = GUI_VERTEX_SHAPE | GUI_VERTEX_SHADOW,
                        .param = 10.0f};
  const ImageData image = capture(style);
  const int inside = shown(image, 50, 30);
  const int at_edge = shown(image, 10, 30);
  const int beyond = shown(image, 3, 30);
  CHECK(inside == 255);
  CHECK(at_edge > 60);
  CHECK(at_edge < 200);
  CHECK(beyond < at_edge);
}
