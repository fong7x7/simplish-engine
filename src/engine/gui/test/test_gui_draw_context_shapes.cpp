#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>

using namespace eng;

namespace {

/// A draw context over a fresh renderer.
struct ShapeFixture {
  GuiRendererContext renderer;
  GuiDrawContext ctx;

  ShapeFixture() {
    ctx.renderer = &renderer;
    renderer.beginFrame();
  }

  /// The first vertex of quad @p i.
  const GuiVertex& quad(size_t i) const { return renderer.vertices[i * 4]; }
  size_t quads() const { return renderer.vertices.size() / 4; }
};

}  // namespace

TEST_CASE("drawRect paints a gradient fill then a per-side border") {
  ShapeFixture fx;
  fx.ctx.drawRect({.rect = {0, 0, 100, 40},
                   .gradient = GuiGradient{.from = {255, 0, 0},
                                           .to = {0, 0, 255},
                                           .angle_degrees = 90.0f},
                   .radii = GuiCorners::top(8.0f),
                   .border = {0.0f, 0.0f, 2.0f, 0.0f},
                   .border_color = {0, 255, 0}});
  REQUIRE(fx.quads() == 2);
  const GuiVertex& fill = fx.quad(0);
  CHECK((fill.flags & GUI_VERTEX_LINEAR_GRADIENT) != 0);
  CHECK(fill.color == GuiColor{255, 0, 0}.pack());
  CHECK(fill.color2 == GuiColor{0, 0, 255}.pack());
  // CSS's 90deg, to the right, is angle 0 in the shader.
  CHECK(fill.param == 0.0f);
  CHECK(fill.radii[0] == 8.0f);
  CHECK(fill.radii[2] == 0.0f);
  const GuiVertex& ring = fx.quad(1);
  CHECK(ring.border[2] == 2.0f);
  CHECK(ring.border[0] == 0.0f);
}

TEST_CASE("radii are no more than half the shorter side") {
  ShapeFixture fx;
  fx.ctx.drawRect({.rect = {0, 0, 100, 20},
                   .fill = {1, 2, 3},
                   .radii = GuiCorners::all(50.0f)});
  CHECK(fx.quad(0).radii[0] == 10.0f);
}

TEST_CASE("a shadow's quad is its box moved, spread and grown by its blur") {
  ShapeFixture fx;
  fx.ctx.drawShadow({10, 10, 100, 50}, GuiCorners::all(6.0f),
                    {.offset_y = 4.0f,
                     .blur = 12.0f,
                     .spread = 2.0f,
                     .color = {0, 0, 0, 100}});
  REQUIRE(fx.quads() == 1);
  const GuiVertex& v = fx.quad(0);
  CHECK((v.flags & GUI_VERTEX_SHADOW) != 0);
  CHECK(v.pos[0] == 10.0f - 14.0f);
  CHECK(v.pos[1] == 14.0f - 14.0f);
  CHECK(v.rect_w == 100.0f + 28.0f);
  CHECK(v.radii[0] == 8.0f);
  CHECK(v.param == 12.0f);
}

TEST_CASE("drawBox draws the theme's shadow for a raised style first") {
  ShapeFixture fx;
  fx.ctx.drawBox({0, 0, 100, 50},
                 GuiTheme::dark().card.of(GuiWidgetState::NORMAL), 1.0f);
  REQUIRE(fx.quads() == 3);  // shadow, fill, border
  CHECK((fx.quad(0).flags & GUI_VERTEX_SHADOW) != 0);
  CHECK((fx.quad(1).flags & GUI_VERTEX_SHADOW) == 0);
}

TEST_CASE("a nine-slice keeps its corners and stretches its middle") {
  ShapeFixture fx;
  fx.ctx.drawNineSlice({0, 0, 200, 100},
                       {.texture = 7,
                        .texture_w = 32,
                        .texture_h = 32,
                        .insets = {8, 8, 8, 8},
                        .scale = 2.0f},
                       {255, 255, 255});
  REQUIRE(fx.quads() == 9);
  // The top-left corner: 16 pixels, the first quarter of the texture.
  const GuiVertex& corner = fx.renderer.vertices[2];
  CHECK(corner.pos[0] == 16.0f);
  CHECK(corner.uv[0] == 0.25f);
  // The middle stretches across the rest.
  const GuiVertex& middle = fx.renderer.vertices[4 * 4 + 2];
  CHECK(middle.pos[0] == 200.0f - 16.0f);
  CHECK(middle.uv[0] == 0.75f);
}

TEST_CASE("a nine-slice smaller than its corners shrinks them to fit") {
  ShapeFixture fx;
  fx.ctx.drawNineSlice({0, 0, 20, 20},
                       {.texture = 7,
                        .texture_w = 32,
                        .texture_h = 32,
                        .insets = {16, 16, 16, 16}},
                       {255, 255, 255});
  // The corners meet; the middle has no size and is not drawn.
  CHECK(fx.quads() == 4);
  CHECK(fx.renderer.vertices[2].pos[0] == 10.0f);
}
