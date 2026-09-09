#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-view-matrix.h>

using Catch::Approx;
using namespace eng::editor;

namespace {

constexpr eng::Rect VIEWPORT{0.0f, 90.0f, 1280.0f, 710.0f};
constexpr IsoViewTarget TARGET{VIEWPORT, 1280.0f, 800.0f};

/// Project a world point through the matrix and convert back to the layout
/// pixel it lands on, which is what `worldToScreen` returns directly.
IsoPoint matrixToScreen(const eng::Mat4& mvp, WorldPoint world) {
  const float clip_x = mvp(0, 0) * world.x + mvp(0, 1) * world.y +
                       mvp(0, 2) * world.z + mvp(0, 3);
  const float clip_y = mvp(1, 0) * world.x + mvp(1, 1) * world.y +
                       mvp(1, 2) * world.z + mvp(1, 3);
  return {(clip_x + 1.0f) * 0.5f * TARGET.layout_width,
          (1.0f - clip_y) * 0.5f * TARGET.layout_height};
}

/// Clip-space depth of a world point.
float matrixDepth(const eng::Mat4& mvp, WorldPoint world) {
  return mvp(2, 0) * world.x + mvp(2, 1) * world.y + mvp(2, 2) * world.z +
         mvp(2, 3);
}

IsoView viewAt(float zoom, IsoPoint focus) {
  IsoCamera camera;
  camera.zoom = zoom;
  camera.focus = focus;
  return makeIsoView(camera, VIEWPORT);
}

}  // namespace

TEST_CASE("the matrix agrees with worldToScreen") {
  // The whole point: a mesh has to land exactly where the 2D grid says its
  // tile is, or geometry and overlays disagree on where the world is.
  const IsoView view = viewAt(1.0f, {0.0f, 0.0f});
  const eng::Mat4 mvp = makeIsoViewProjection(view, TARGET);

  const WorldPoint points[] = {
      {0.0f, 0.0f}, {3.0f, 5.0f}, {-2.5f, 7.25f}, {4.0f, 1.0f, 2.0f}};
  for (const auto& world : points) {
    const IsoPoint expected = worldToScreen(view, world);
    const IsoPoint actual = matrixToScreen(mvp, world);
    REQUIRE(actual.x == Approx(expected.x).margin(1e-2));
    REQUIRE(actual.y == Approx(expected.y).margin(1e-2));
  }
}

TEST_CASE("the matrix agrees with worldToScreen at any zoom and pan") {
  for (float zoom : {0.25f, 1.0f, 3.0f}) {
    const IsoView view = viewAt(zoom, {120.0f, -340.0f});
    const eng::Mat4 mvp = makeIsoViewProjection(view, TARGET);

    const WorldPoint world{6.5f, -3.25f, 1.5f};
    const IsoPoint expected = worldToScreen(view, world);
    const IsoPoint actual = matrixToScreen(mvp, world);
    REQUIRE(actual.x == Approx(expected.x).margin(1e-2));
    REQUIRE(actual.y == Approx(expected.y).margin(1e-2));
  }
}

TEST_CASE("a tile nearer the viewer is nearer in depth") {
  const eng::Mat4 mvp = makeIsoViewProjection(viewAt(1.0f, {}), TARGET);

  // +Y runs down the screen, toward the viewer, as in every 2.5D game.
  REQUIRE(matrixDepth(mvp, {0.0f, 6.0f}) < matrixDepth(mvp, {0.0f, 5.0f}));
}

TEST_CASE("a taller point is nearer in depth") {
  const eng::Mat4 mvp = makeIsoViewProjection(viewAt(1.0f, {}), TARGET);

  // The top of a wall has to occlude what stands behind its base.
  REQUIRE(matrixDepth(mvp, {0.0f, 5.0f, 3.0f}) <
          matrixDepth(mvp, {0.0f, 5.0f, 0.0f}));
}

TEST_CASE("points along the projection ray order by depth") {
  const eng::Mat4 mvp = makeIsoViewProjection(viewAt(1.0f, {}), TARGET);
  // Points collapse to one pixel along `isoProjectionRay`. Two points on
  // that ray must land on the same pixel and differ only in depth, or the
  // depth buffer cannot tell them apart. Derived rather than written out:
  // the ray is a property of the projection, not a constant.
  const WorldPoint ray = isoProjectionRay(ISO_AXES_DIMETRIC);
  const WorldPoint near_point{2.0f, 5.0f, 0.0f};
  const WorldPoint far_point{2.0f, 5.0f - ray.y / ray.z, -1.0f};

  const IsoPoint a = matrixToScreen(mvp, near_point);
  const IsoPoint b = matrixToScreen(mvp, far_point);
  REQUIRE(a.x == Approx(b.x).margin(1e-2));
  REQUIRE(a.y == Approx(b.y).margin(1e-2));
  REQUIRE(matrixDepth(mvp, near_point) < matrixDepth(mvp, far_point));
}

TEST_CASE("the ground plane at the origin sits mid-range in depth") {
  const eng::Mat4 mvp = makeIsoViewProjection(viewAt(1.0f, {}), TARGET);
  REQUIRE(matrixDepth(mvp, {0.0f, 0.0f, 0.0f}) == Approx(0.5f));
}

TEST_CASE("depth stays inside the clip range across a large level") {
  const eng::Mat4 mvp = makeIsoViewProjection(viewAt(1.0f, {}), TARGET);
  for (float tile : {-512.0f, -64.0f, 0.0f, 64.0f, 512.0f}) {
    const float depth = matrixDepth(mvp, {0.0f, tile, tile});
    REQUIRE(depth > 0.0f);
    REQUIRE(depth < 1.0f);
  }
}
