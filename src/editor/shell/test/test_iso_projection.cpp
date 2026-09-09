#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-projection.h>

using Catch::Approx;
using namespace eng::editor;

namespace {
constexpr eng::Rect VIEWPORT{0.0f, 0.0f, 800.0f, 600.0f};
constexpr IsoAxes DIMETRIC = ISO_AXES_DIMETRIC;
constexpr IsoAxes ISOMETRIC = ISO_AXES_ISOMETRIC;
}  // namespace

TEST_CASE("the dimetric tile footprint is 4:3") {
  // ADR-003 fixes this ratio; sprite art is authored against it.
  REQUIRE(DIMETRIC.y_down == Approx(ISO_TILE_WIDTH * 0.75f));
}

/// The length of the row of the projection matrix that makes screen X, and
/// of the row that makes screen Y. A projection keeps shapes exactly when
/// these are equal and the rows are perpendicular.
float acrossRowLength(const IsoAxes& axes) {
  return std::hypot(axes.x_across, axes.y_across);
}
float downRowLength(const IsoAxes& axes) {
  return std::sqrt(axes.x_down * axes.x_down + axes.y_down * axes.y_down +
                   axes.z_up * axes.z_up);
}

TEST_CASE("neither projection stretches what it draws") {
  // The property that makes a sphere draw round rather than as an oval.
  // Both projections used to fail this — the height axis was picked to be
  // unforeshortened instead of being derived — and it showed up as every
  // round object drawing 1.25 (dimetric) or 1.5 (isometric) times taller
  // than it was wide.
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    REQUIRE(downRowLength(axes) == Approx(acrossRowLength(axes)));
    // Perpendicular, or the projection would shear as well as stretch.
    REQUIRE(axes.x_across * axes.x_down + axes.y_across * axes.y_down ==
            Approx(0.0f).margin(1e-3));
  }
}

TEST_CASE("the height axis is foreshortened by the camera's pitch") {
  // A camera tilted far enough to show any ground at all sees a vertical
  // face shortened. Only a projection that is a shear rather than a view
  // can leave height unforeshortened, which is what this used to be.
  REQUIRE(DIMETRIC.z_up < ISO_TILE_WIDTH);
  REQUIRE(DIMETRIC.z_up == Approx(42.332f).margin(1e-2));
  REQUIRE(ISOMETRIC.z_up == Approx(39.192f).margin(1e-2));
}

TEST_CASE("the isometric tile is a 2:1 diamond") {
  // 64 across by 32 down, the shape every isometric tileset is cut to.
  REQUIRE(ISOMETRIC.x_across - ISOMETRIC.y_across == Approx(ISO_TILE_WIDTH));
  REQUIRE(ISOMETRIC.x_down + ISOMETRIC.y_down == Approx(ISO_TILE_WIDTH * 0.5f));
}

TEST_CASE("a tile is the same width in both projections") {
  // What a project keeps across a switch is the tile, not the height: the
  // isometric camera is pitched differently, so it sees a wall differently.
  REQUIRE(acrossRowLength(DIMETRIC) == Approx(ISO_TILE_WIDTH));
  REQUIRE(ISOMETRIC.x_across - ISOMETRIC.y_across == Approx(ISO_TILE_WIDTH));
}

TEST_CASE("the projection each name selects round-trips") {
  REQUIRE(isoAxesFor(ProjectProjection::DIMETRIC).y_across ==
          Approx(DIMETRIC.y_across));
  REQUIRE(isoAxesFor(ProjectProjection::ISOMETRIC).y_across ==
          Approx(ISOMETRIC.y_across));
}

TEST_CASE("worldToIso places the origin at the isometric origin") {
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    const IsoPoint origin = worldToIso(axes, {0.0f, 0.0f});
    REQUIRE(origin.x == Approx(0.0f));
    REQUIRE(origin.y == Approx(0.0f));
  }
}

TEST_CASE("the dimetric ground axes are axis-aligned on screen") {
  const IsoPoint x_axis = worldToIso(DIMETRIC, {1.0f, 0.0f});
  const IsoPoint y_axis = worldToIso(DIMETRIC, {0.0f, 1.0f});

  // Zero yaw: +X goes straight right, +Y goes straight down. Neither leaks
  // into the other screen axis, so tiles are rectangles, not diamonds.
  REQUIRE(x_axis.x == Approx(ISO_TILE_WIDTH));
  REQUIRE(x_axis.y == Approx(0.0f));
  REQUIRE(y_axis.x == Approx(0.0f));
  REQUIRE(y_axis.y == Approx(DIMETRIC.y_down));
}

TEST_CASE("the isometric ground axes both run diagonally") {
  const IsoPoint x_axis = worldToIso(ISOMETRIC, {1.0f, 0.0f});
  const IsoPoint y_axis = worldToIso(ISOMETRIC, {0.0f, 1.0f});

  // 45 degrees of yaw: +X goes right and down, +Y goes left and down, both
  // by the same amounts. That mirror symmetry is what makes tiles diamonds.
  REQUIRE(x_axis.x == Approx(-y_axis.x));
  REQUIRE(x_axis.y == Approx(y_axis.y));
  REQUIRE(x_axis.y > 0.0f);
}

TEST_CASE("world height projects straight up the screen") {
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    const IsoPoint raised = worldToIso(axes, {0.0f, 0.0f, 1.0f});

    // +Z rises with no horizontal shear, in either projection.
    REQUIRE(raised.x == Approx(0.0f));
    REQUIRE(raised.y == Approx(-axes.z_up));
  }
}

TEST_CASE("height is independent of ground position") {
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    const IsoPoint ground = worldToIso(axes, {3.0f, 4.0f});
    const IsoPoint raised = worldToIso(axes, {3.0f, 4.0f, 2.0f});

    REQUIRE(raised.x == Approx(ground.x));
    REQUIRE(ground.y - raised.y == Approx(2.0f * axes.z_up));
  }
}

TEST_CASE("isoToWorld inverts worldToIso on the ground plane") {
  const WorldPoint inputs[] = {
      {0.0f, 0.0f}, {3.0f, 7.0f}, {-4.0f, 2.5f}, {12.25f, -9.75f}};
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    for (const auto& world : inputs) {
      const WorldPoint round_trip = isoToWorld(axes, worldToIso(axes, world));
      REQUIRE(round_trip.x == Approx(world.x).margin(1e-3));
      REQUIRE(round_trip.y == Approx(world.y).margin(1e-3));
      REQUIRE(round_trip.z == Approx(0.0f));
    }
  }
}

TEST_CASE("isoToWorld picks the ground plane under a raised point") {
  // A raised tile projects to the same screen point as a ground tile
  // further back along the projection ray. The inverse resolves that
  // ambiguity by always answering with the ground tile.
  const WorldPoint ground =
      isoToWorld(DIMETRIC, worldToIso(DIMETRIC, {2.0f, 6.0f, 1.0f}));
  REQUIRE(ground.x == Approx(2.0f));
  REQUIRE(ground.y == Approx(6.0f - DIMETRIC.z_up / DIMETRIC.y_down));
  REQUIRE(ground.z == Approx(0.0f));
}

TEST_CASE("the projection ray lands back on the same pixel") {
  // The defining property: a point moved along the ray does not move on
  // screen, which is what makes it the axis to pick and sort depth along.
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    const WorldPoint ray = isoProjectionRay(axes);
    const IsoPoint at = worldToIso(axes, {1.0f, 2.0f, 0.5f});
    const IsoPoint along =
        worldToIso(axes, {1.0f + ray.x, 2.0f + ray.y, 0.5f + ray.z});

    REQUIRE(along.x == Approx(at.x).margin(1e-3));
    REQUIRE(along.y == Approx(at.y).margin(1e-3));
  }
}

TEST_CASE("the projection ray points toward the viewer") {
  // Both ground axes run down the screen toward the viewer, and up is up,
  // so every component is positive. A sign slip here would sort the scene
  // back to front.
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    const WorldPoint ray = isoProjectionRay(axes);
    REQUIRE(ray.y > 0.0f);
    REQUIRE(ray.z > 0.0f);
    REQUIRE(ray.x >= 0.0f);
  }
}

TEST_CASE("the camera focus lands at the viewport centre") {
  IsoCamera camera;
  camera.focus = worldToIso(camera.axes, {5.0f, 5.0f});
  const IsoView view = makeIsoView(camera, VIEWPORT);

  const IsoPoint screen = worldToScreen(view, {5.0f, 5.0f});
  REQUIRE(screen.x == Approx(VIEWPORT.x + VIEWPORT.w * 0.5f));
  REQUIRE(screen.y == Approx(VIEWPORT.y + VIEWPORT.h * 0.5f));
}

TEST_CASE("the camera carries its projection into the view") {
  IsoCamera camera;
  camera.axes = ISO_AXES_ISOMETRIC;
  const IsoView view = makeIsoView(camera, VIEWPORT);

  // Without this the menu could switch the setting and nothing would move.
  REQUIRE(view.axes.y_across == Approx(ISOMETRIC.y_across));
}

TEST_CASE("screenToWorld inverts worldToScreen at any zoom") {
  for (const IsoAxes& axes : {DIMETRIC, ISOMETRIC}) {
    for (float zoom : {0.25f, 1.0f, 2.5f, 4.0f}) {
      IsoCamera camera;
      camera.axes = axes;
      camera.zoom = zoom;
      camera.focus = worldToIso(axes, {2.0f, -3.0f});
      const IsoView view = makeIsoView(camera, VIEWPORT);

      const WorldPoint world{6.5f, -1.25f};
      const IsoPoint screen = worldToScreen(view, world);
      const WorldPoint back = screenToWorld(view, screen);
      REQUIRE(back.x == Approx(world.x).margin(1e-3));
      REQUIRE(back.y == Approx(world.y).margin(1e-3));
    }
  }
}

TEST_CASE("zoom scales the on-screen tile size") {
  IsoCamera camera;
  camera.zoom = 2.0f;
  const IsoView view = makeIsoView(camera, VIEWPORT);

  const IsoPoint a = worldToScreen(view, {0.0f, 0.0f});
  const IsoPoint b = worldToScreen(view, {1.0f, 1.0f});
  REQUIRE(b.x - a.x == Approx(ISO_TILE_WIDTH * 2.0f));
  REQUIRE(b.y - a.y == Approx(DIMETRIC.y_down * 2.0f));
}
