#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-projection.h>

using Catch::Approx;
using namespace eng::editor;

namespace {
constexpr eng::Rect VIEWPORT{0.0f, 0.0f, 800.0f, 600.0f};
}

TEST_CASE("the tile footprint is 4:3 dimetric") {
  // ADR-003 fixes this ratio; sprite art is authored against it.
  REQUIRE(ISO_TILE_DEPTH == Approx(ISO_TILE_WIDTH * 0.75f));
}

TEST_CASE("the height axis is unforeshortened") {
  // X and Z share a scale while Y is foreshortened: that is what makes the
  // projection dimetric, and what makes vertical faces read as seen head-on.
  REQUIRE(ISO_TILE_RISE == Approx(ISO_TILE_WIDTH));
  REQUIRE(ISO_TILE_RISE != Approx(ISO_TILE_DEPTH));
}

TEST_CASE("worldToIso places the origin at the isometric origin") {
  const IsoPoint origin = worldToIso({0.0f, 0.0f});
  REQUIRE(origin.x == Approx(0.0f));
  REQUIRE(origin.y == Approx(0.0f));
}

TEST_CASE("the ground axes are axis-aligned on screen") {
  const IsoPoint x_axis = worldToIso({1.0f, 0.0f});
  const IsoPoint y_axis = worldToIso({0.0f, 1.0f});

  // Zero yaw: +X goes straight right, +Y goes straight down. Neither leaks
  // into the other screen axis, so tiles are rectangles, not diamonds.
  REQUIRE(x_axis.x == Approx(ISO_TILE_WIDTH));
  REQUIRE(x_axis.y == Approx(0.0f));
  REQUIRE(y_axis.x == Approx(0.0f));
  REQUIRE(y_axis.y == Approx(ISO_TILE_DEPTH));
}

TEST_CASE("world height projects straight up the screen") {
  const IsoPoint raised = worldToIso({0.0f, 0.0f, 1.0f});

  // +Z rises with no horizontal shear, at the same scale as +X.
  REQUIRE(raised.x == Approx(0.0f));
  REQUIRE(raised.y == Approx(-ISO_TILE_RISE));
}

TEST_CASE("height is independent of ground position") {
  const IsoPoint ground = worldToIso({3.0f, 4.0f});
  const IsoPoint raised = worldToIso({3.0f, 4.0f, 2.0f});

  REQUIRE(raised.x == Approx(ground.x));
  REQUIRE(ground.y - raised.y == Approx(2.0f * ISO_TILE_RISE));
}

TEST_CASE("isoToWorld inverts worldToIso on the ground plane") {
  const WorldPoint inputs[] = {
      {0.0f, 0.0f}, {3.0f, 7.0f}, {-4.0f, 2.5f}, {12.25f, -9.75f}};
  for (const auto& world : inputs) {
    const WorldPoint round_trip = isoToWorld(worldToIso(world));
    REQUIRE(round_trip.x == Approx(world.x));
    REQUIRE(round_trip.y == Approx(world.y));
    REQUIRE(round_trip.z == Approx(0.0f));
  }
}

TEST_CASE("isoToWorld picks the ground plane under a raised point") {
  // A raised tile projects to the same screen point as a ground tile
  // ISO_TILE_RISE / ISO_TILE_DEPTH tiles further back. The inverse resolves
  // that ambiguity by always answering with the ground tile.
  const WorldPoint ground = isoToWorld(worldToIso({2.0f, 6.0f, 1.0f}));
  REQUIRE(ground.x == Approx(2.0f));
  REQUIRE(ground.y == Approx(6.0f - ISO_TILE_RISE / ISO_TILE_DEPTH));
  REQUIRE(ground.z == Approx(0.0f));
}

TEST_CASE("the camera focus lands at the viewport centre") {
  IsoCamera camera;
  camera.focus = worldToIso({5.0f, 5.0f});
  const IsoView view = makeIsoView(camera, VIEWPORT);

  const IsoPoint screen = worldToScreen(view, {5.0f, 5.0f});
  REQUIRE(screen.x == Approx(VIEWPORT.x + VIEWPORT.w * 0.5f));
  REQUIRE(screen.y == Approx(VIEWPORT.y + VIEWPORT.h * 0.5f));
}

TEST_CASE("screenToWorld inverts worldToScreen at any zoom") {
  for (float zoom : {0.25f, 1.0f, 2.5f, 4.0f}) {
    IsoCamera camera;
    camera.zoom = zoom;
    camera.focus = worldToIso({2.0f, -3.0f});
    const IsoView view = makeIsoView(camera, VIEWPORT);

    const WorldPoint world{6.5f, -1.25f};
    const IsoPoint screen = worldToScreen(view, world);
    const WorldPoint back = screenToWorld(view, screen);
    REQUIRE(back.x == Approx(world.x).margin(1e-3));
    REQUIRE(back.y == Approx(world.y).margin(1e-3));
  }
}

TEST_CASE("zoom scales the on-screen tile size") {
  IsoCamera camera;
  camera.zoom = 2.0f;
  const IsoView view = makeIsoView(camera, VIEWPORT);

  const IsoPoint a = worldToScreen(view, {0.0f, 0.0f});
  const IsoPoint b = worldToScreen(view, {1.0f, 1.0f});
  REQUIRE(b.x - a.x == Approx(ISO_TILE_WIDTH * 2.0f));
  REQUIRE(b.y - a.y == Approx(ISO_TILE_DEPTH * 2.0f));
}
