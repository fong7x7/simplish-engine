#include <catch2/catch_test_macros.hpp>

// The conversion is pure, but it lives with the OpenGL backend and is
// compiled only where that backend is.
#ifdef ENGINE_RENDERER_OPENGL

#include <engine/render/backends/opengl/gl-window-rect.h>

using namespace eng;
using namespace eng::render;

namespace {

/// Height of the framebuffer every case here draws into.
constexpr uint32_t SURFACE_H = 600;

}  // namespace

TEST_CASE("a scissor near the top clips near the top on GL too",
          "[opengl][window-rect]") {
  // Twenty rows down and a hundred tall from the top is, counted from the
  // bottom, everything above row 480. Passed through unturned it would
  // have clipped rows 20 to 120 from the bottom instead.
  const GlWindowRect r = glScissorRect({10, 20, 300, 100}, SURFACE_H);
  REQUIRE(r.x == 10);
  REQUIRE(r.y == 480);
  REQUIRE(r.width == 300);
  REQUIRE(r.height == 100);
}

TEST_CASE("a scissor covering the whole surface is unchanged",
          "[opengl][window-rect]") {
  const GlWindowRect r = glScissorRect({0, 0, 800, SURFACE_H}, SURFACE_H);
  REQUIRE(r.x == 0);
  REQUIRE(r.y == 0);
  REQUIRE(r.height == static_cast<int>(SURFACE_H));
}

TEST_CASE("a scissor on the bottom edge starts at GL's row zero",
          "[opengl][window-rect]") {
  const GlWindowRect r = glScissorRect({0, 550, 800, 50}, SURFACE_H);
  REQUIRE(r.y == 0);
}

TEST_CASE("a full-surface viewport is the same either way up",
          "[opengl][window-rect]") {
  // Every viewport the GUI and the mesh pass set today covers the whole
  // surface, so turning them over changes nothing that already drew.
  const GlWindowRect r = glViewportRect(
      {0.0f, 0.0f, 800.0f, static_cast<float>(SURFACE_H), 0.0f, 1.0f},
      SURFACE_H);
  REQUIRE(r.x == 0);
  REQUIRE(r.y == 0);
  REQUIRE(r.width == 800);
  REQUIRE(r.height == static_cast<int>(SURFACE_H));
}

TEST_CASE("a viewport near the top is turned over like a scissor",
          "[opengl][window-rect]") {
  const GlWindowRect r =
      glViewportRect({10.0f, 20.0f, 300.0f, 100.0f, 0.0f, 1.0f}, SURFACE_H);
  REQUIRE(r.x == 10);
  REQUIRE(r.y == 480);
  REQUIRE(r.height == 100);
}

#endif  // ENGINE_RENDERER_OPENGL
