#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-placement-pick.h>
#include <editor/shell/iso-camera.h>

using Catch::Approx;
using namespace eng::editor;

namespace {

IsoView viewOf(const eng::Rect& rect) {
  IsoCamera camera;
  return makeIsoView(camera, rect);
}

/// The same viewport seen with a project's chosen projection.
IsoView viewOf(const eng::Rect& rect, ProjectProjection projection) {
  IsoCamera camera;
  camera.axes = isoAxesFor(projection);
  return makeIsoView(camera, rect);
}

/// A marker one tile square, standing @p height tall on the ground.
EditorPlacementMarker tileMarker(float x, float y, float height = 1.0f) {
  return {{{x, y, 0.0f}, {x + 1.0f, y + 1.0f, height}}, false};
}

/// Where a marker's own centre lands on screen.
IsoPoint centreOf(const IsoView& view, const EditorPlacementMarker& marker) {
  const PlacementBounds& bounds = marker.bounds;
  return worldToScreen(view, {(bounds.min.x + bounds.max.x) * 0.5f,
                              (bounds.min.y + bounds.max.y) * 0.5f,
                              (bounds.min.z + bounds.max.z) * 0.5f});
}

}  // namespace

TEST_CASE("the pick ray is the direction the projection collapses along") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  // Two world points a step apart along the ray land on the same pixel,
  // which is the property that makes it the ray to pick along.
  const WorldPoint ray = isoProjectionRay(view.axes);
  const IsoPoint low = worldToScreen(view, {2.0f, 3.0f, 0.0f});
  const IsoPoint high =
      worldToScreen(view, {2.0f + ray.x, 3.0f + ray.y, 0.0f + ray.z});
  REQUIRE(low.x == Approx(high.x));
  REQUIRE(low.y == Approx(high.y));
}

TEST_CASE("picking follows the projection the project chose") {
  // The isometric ray has an X component the dimetric one does not, so a
  // pick that still used the old constants would miss by a whole tile.
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f),
                              ProjectProjection::ISOMETRIC);
  const std::vector<EditorPlacementMarker> markers{tileMarker(3.0f, -2.0f)};
  REQUIRE(pickPlacementMarker(view, markers, centreOf(view, markers[0])) == 0);
}

TEST_CASE("a box the ray misses reports no hit at all") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  const PlacementBounds bounds{{2.0f, 3.0f, 0.0f}, {3.0f, 4.0f, 2.0f}};
  // Off to the side, where no amount of travel along the ray reaches it:
  // the ray has no X component, so a click beside a box never finds it.
  const IsoPoint beside = worldToScreen(view, {8.0f, 3.5f, 1.0f});
  REQUIRE_FALSE(placementRayHit(view, bounds, beside).has_value());
}

TEST_CASE("a click on a placement picks it") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  const std::vector<EditorPlacementMarker> markers{tileMarker(0.0f, 0.0f)};
  REQUIRE(pickPlacementMarker(view, markers, centreOf(view, markers[0])) == 0);
}

TEST_CASE("a click on bare ground picks nothing") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  const std::vector<EditorPlacementMarker> markers{tileMarker(0.0f, 0.0f)};
  const IsoPoint far_away = worldToScreen(view, {20.0f, 20.0f, 0.0f});
  REQUIRE(pickPlacementMarker(view, markers, far_away) ==
          EDITOR_PLACEMENT_NONE);
}

TEST_CASE("an empty world picks nothing") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  REQUIRE(pickPlacementMarker(view, {}, {400.0f, 300.0f}) ==
          EDITOR_PLACEMENT_NONE);
}

TEST_CASE("the nearer of two overlapping placements is picked") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  // A tall thing behind and a short thing in front of it. Their screen
  // boxes overlap, and the one in front is the one whose mesh is visible.
  const std::vector<EditorPlacementMarker> markers{
      tileMarker(0.0f, 0.0f, 4.0f),
      tileMarker(0.0f, 1.0f, 1.0f),
  };
  const IsoPoint overlap = centreOf(view, markers[1]);
  REQUIRE(pickPlacementMarker(view, markers, overlap) == 1);
}

TEST_CASE("list order does not decide an overlap") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  // The same two, dropped in the other order. Whichever was placed first,
  // the near one still wins.
  const std::vector<EditorPlacementMarker> markers{
      tileMarker(0.0f, 1.0f, 1.0f),
      tileMarker(0.0f, 0.0f, 4.0f),
  };
  const IsoPoint overlap = centreOf(view, markers[0]);
  REQUIRE(pickPlacementMarker(view, markers, overlap) == 0);
}

TEST_CASE("a placement raised off the ground is picked where it is drawn") {
  const IsoView view = viewOf(eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f));
  const std::vector<EditorPlacementMarker> markers{
      {{{0.0f, 0.0f, 3.0f}, {1.0f, 1.0f, 4.0f}}, false}};

  REQUIRE(pickPlacementMarker(view, markers, centreOf(view, markers[0])) == 0);
  // Its tile is still on the ground, but the box is three units up the
  // screen from it, and that is where a click has to land.
  const IsoPoint on_the_tile = worldToScreen(view, {0.5f, 0.5f, 0.0f});
  REQUIRE(pickPlacementMarker(view, markers, on_the_tile) ==
          EDITOR_PLACEMENT_NONE);
}

TEST_CASE("panning the camera moves what a screen point picks") {
  const eng::Rect rect = eng::makeRect(0.0f, 0.0f, 800.0f, 600.0f);
  IsoCamera camera;
  camera.focus = worldToIso(camera.axes, {8.0f, 8.0f});
  const IsoView panned = makeIsoView(camera, rect);
  const std::vector<EditorPlacementMarker> markers{tileMarker(0.0f, 0.0f)};

  // The placement is off screen once the camera is elsewhere, so the point
  // that used to hit it hits nothing.
  const IsoPoint was_a_hit = centreOf(viewOf(rect), markers[0]);
  REQUIRE(pickPlacementMarker(panned, markers, was_a_hit) ==
          EDITOR_PLACEMENT_NONE);
  REQUIRE(pickPlacementMarker(panned, markers, centreOf(panned, markers[0])) ==
          0);
}
