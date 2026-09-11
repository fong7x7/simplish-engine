#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <limits>
#include <string>
#include <vector>

using namespace eng;
using namespace eng::editor;

namespace {

/// A document holding @p waypoints, in that order.
EditorDocument documentWith(std::initializer_list<EditorWaypoint> waypoints) {
  EditorDocument document;
  for (const EditorWaypoint& waypoint : waypoints) {
    document.waypoints.push_back(waypoint);
  }
  return document;
}

/// A waypoint of @p route at place @p order, standing at x = @p x.
EditorWaypoint at(uint8_t route, uint16_t order, float x) {
  return makeEditorWaypoint(route, order, {x, 0.5f, 0.0f});
}

}  // namespace

TEST_CASE("a route is rounded to a whole route and clamped to the nine") {
  REQUIRE(clampEditorRoute(2.4f) == 2);
  REQUIRE(clampEditorRoute(2.6f) == 3);
  REQUIRE(clampEditorRoute(0.0f) == 1);
  REQUIRE(clampEditorRoute(40.0f) == EDITOR_ROUTE_COUNT);
  REQUIRE(clampEditorRoute(std::numeric_limits<float>::quiet_NaN()) == 1);
}

TEST_CASE("a waypoint's place is rounded and clamped") {
  REQUIRE(clampEditorWaypointOrder(3.4f) == 3);
  REQUIRE(clampEditorWaypointOrder(-2.0f) == 1);
  REQUIRE(clampEditorWaypointOrder(500.0f) == EDITOR_WAYPOINT_MAX_ORDER);
}

TEST_CASE("the next place is after the route's last, and other routes do not "
          "count") {
  const EditorDocument document =
      documentWith({at(1, 1, 0.0f), at(1, 4, 1.0f), at(2, 9, 2.0f)});
  REQUIRE(nextEditorWaypointOrder(document, 1) == 5);
  REQUIRE(nextEditorWaypointOrder(document, 2) == 10);
  REQUIRE(nextEditorWaypointOrder(document, 3) == 1);
}

TEST_CASE(
    "a route is walked by place, then by list order where two share one") {
  const EditorDocument document = documentWith(
      {at(1, 3, 3.0f), at(2, 1, 9.0f), at(1, 1, 1.0f), at(1, 3, 4.0f)});

  const std::vector<Vec2> points = editorRoutePoints(document, 1);

  REQUIRE(points.size() == 3);
  REQUIRE(points[0].x == 1.0f);
  REQUIRE(points[1].x == 3.0f);
  REQUIRE(points[2].x == 4.0f);
  REQUIRE(editorRoutePoints(document, 5).empty());
}

TEST_CASE("the routes in use are listed lowest first") {
  const EditorDocument document =
      documentWith({at(4, 1, 0.0f), at(2, 1, 0.0f), at(4, 2, 0.0f)});
  REQUIRE(editorRoutesInUse(document) == std::vector<uint8_t>{2, 4});
  REQUIRE(editorRoutesInUse(EditorDocument{}).empty());
}

TEST_CASE("the Route row offers none and every route in use") {
  const EditorDocument document = documentWith({at(3, 1, 0.0f)});

  const EditorRouteChoices none = editorRouteChoices(document, 0);
  REQUIRE(none.names == std::vector<std::string>{"None", "Route 3"});
  REQUIRE(none.routes == std::vector<uint8_t>{0, 3});
  REQUIRE(none.current == 0);
  REQUIRE(editorRouteChoices(document, 3).current == 1);
}

TEST_CASE("a route with no waypoints left is still offered, marked empty") {
  const EditorRouteChoices choices =
      editorRouteChoices(documentWith({at(1, 1, 0.0f)}), 6);
  REQUIRE(choices.names.back() == "Route 6 (empty)");
  REQUIRE(choices.routes.back() == 6);
  REQUIRE(choices.current == 2);
}

TEST_CASE("a waypoint's fields are its route, its place and its position") {
  EditorWaypoint waypoint = at(1, 1, 2.0f);
  setEditorWaypointValue(waypoint, EditorPropertyField::ROUTE, 12.0f);
  setEditorWaypointValue(waypoint, EditorPropertyField::ORDER, 2.6f);
  setEditorWaypointValue(waypoint, EditorPropertyField::POSITION_Y, 7.0f);
  setEditorWaypointValue(waypoint, EditorPropertyField::SCALE, 3.0f);

  REQUIRE(waypoint.route == EDITOR_ROUTE_COUNT);
  REQUIRE(waypoint.order == 3);
  REQUIRE(editorWaypointValue(waypoint, EditorPropertyField::POSITION_Y) ==
          7.0f);
  REQUIRE(editorWaypointValue(waypoint, EditorPropertyField::SCALE) == 0.0f);
  REQUIRE(editorWaypointHasField(EditorPropertyField::ORDER));
  REQUIRE_FALSE(editorWaypointHasField(EditorPropertyField::PLAYER));
}

TEST_CASE("a waypoint is named by its route and its place") {
  REQUIRE(editorWaypointName(at(2, 3, 0.0f)) == "Route 2 · Waypoint 3");
}

TEST_CASE("a waypoint is picked as a short post standing on its position") {
  const PlacementBounds bounds = editorWaypointBounds(at(1, 1, 2.0f));
  REQUIRE(bounds.min.x < 2.0f);
  REQUIRE(bounds.max.x > 2.0f);
  REQUIRE(bounds.min.z == 0.0f);
  REQUIRE(bounds.max.z == EDITOR_WAYPOINT_MARKER_HEIGHT);
}

TEST_CASE("waypoint ids are minted in sequence and never reused") {
  EditorDocument document;
  EditorWaypoint first = at(1, 1, 0.0f);
  first.id = mintEditorWaypointId(document);
  document.waypoints.push_back(first);

  REQUIRE(first.id == "waypoint_01");
  REQUIRE(mintEditorWaypointId(document) == "waypoint_02");
  REQUIRE(editorWaypointRef(first) == "waypoint:waypoint_01");
}
