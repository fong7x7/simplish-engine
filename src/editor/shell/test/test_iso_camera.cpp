#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <editor/shell/iso-camera.h>

using Catch::Approx;
using namespace eng::editor;

namespace {
constexpr eng::Rect VIEWPORT{0.0f, 0.0f, 800.0f, 600.0f};
}

TEST_CASE("panCamera moves the focus opposite the drag, scaled by zoom") {
  IsoCamera camera;
  camera.zoom = 2.0f;
  panCamera(camera, 100.0f, 50.0f);

  // Dragging right pulls the world right, so the focus moves left.
  REQUIRE(camera.focus.x == Approx(-50.0f));
  REQUIRE(camera.focus.y == Approx(-25.0f));
}

TEST_CASE("panCamera at low zoom covers more world per pixel") {
  IsoCamera zoomed_in;
  zoomed_in.zoom = 4.0f;
  IsoCamera zoomed_out;
  zoomed_out.zoom = 0.5f;

  panCamera(zoomed_in, 80.0f, 0.0f);
  panCamera(zoomed_out, 80.0f, 0.0f);

  REQUIRE(std::abs(zoomed_out.focus.x) > std::abs(zoomed_in.focus.x));
}

TEST_CASE("zoomCameraAt clamps to the allowed range") {
  IsoCamera camera;

  for (int i = 0; i < 100; ++i) {
    zoomCameraAt(camera, VIEWPORT, 1.0f, 400.0f, 300.0f);
  }
  REQUIRE(camera.zoom == Approx(ISO_ZOOM_MAX));

  for (int i = 0; i < 200; ++i) {
    zoomCameraAt(camera, VIEWPORT, -1.0f, 400.0f, 300.0f);
  }
  REQUIRE(camera.zoom == Approx(ISO_ZOOM_MIN));
}

TEST_CASE("zoomCameraAt keeps the anchored point under the cursor") {
  IsoCamera camera;
  constexpr float ANCHOR_X = 620.0f;
  constexpr float ANCHOR_Y = 180.0f;

  const IsoView before = makeIsoView(camera, VIEWPORT);
  const IsoPoint world_before = screenToIso(before, {ANCHOR_X, ANCHOR_Y});

  zoomCameraAt(camera, VIEWPORT, 3.0f, ANCHOR_X, ANCHOR_Y);

  const IsoView after = makeIsoView(camera, VIEWPORT);
  const IsoPoint world_after = screenToIso(after, {ANCHOR_X, ANCHOR_Y});

  REQUIRE(world_after.x == Approx(world_before.x).margin(1e-2));
  REQUIRE(world_after.y == Approx(world_before.y).margin(1e-2));
  REQUIRE(camera.zoom > 1.0f);
}

TEST_CASE("zoomCameraAt at the clamp leaves the camera unchanged") {
  IsoCamera camera;
  camera.zoom = ISO_ZOOM_MAX;
  const IsoPoint focus_before = camera.focus;

  zoomCameraAt(camera, VIEWPORT, 5.0f, 400.0f, 300.0f);

  REQUIRE(camera.zoom == Approx(ISO_ZOOM_MAX));
  REQUIRE(camera.focus.x == Approx(focus_before.x).margin(1e-3));
  REQUIRE(camera.focus.y == Approx(focus_before.y).margin(1e-3));
}
