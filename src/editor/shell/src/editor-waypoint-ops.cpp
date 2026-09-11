#include "editor-vector-field.h"

#include <algorithm>
#include <cmath>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-property-traits.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <string>

namespace eng::editor {

namespace {

  /// Whether a field names one of a waypoint's position components.
  bool isPositionField(EditorPropertyField field) {
    return editorFieldInTriple(field, EditorPropertyField::POSITION_X);
  }

  /// @p value rounded and held to [1, @p high]; a NaN is 1.
  float wholeFromOne(float value, float high) {
    return std::isnan(value) ? 1.0f : std::clamp(std::round(value), 1.0f, high);
  }

  /// The waypoints of @p route in @p document, in the order listed.
  std::vector<const EditorWaypoint*>
  routeWaypoints(const EditorDocument& document, uint8_t route) {
    std::vector<const EditorWaypoint*> found;
    for (const EditorWaypoint& waypoint : document.waypoints) {
      if (waypoint.route == route) {
        found.push_back(&waypoint);
      }
    }
    return found;
  }

  static_assert(editorPropertyTraits(EditorPropertyField::ROUTE).maximum ==
                    static_cast<float>(EDITOR_ROUTE_COUNT),
                "the Route row holds exactly the routes a level can have");
  static_assert(editorPropertyTraits(EditorPropertyField::ORDER).maximum ==
                    static_cast<float>(EDITOR_WAYPOINT_MAX_ORDER),
                "the Order row holds exactly the places a route has");

}  // namespace

uint8_t clampEditorRoute(float route) {
  return static_cast<uint8_t>(
      wholeFromOne(route, static_cast<float>(EDITOR_ROUTE_COUNT)));
}

uint16_t clampEditorWaypointOrder(float order) {
  return static_cast<uint16_t>(
      wholeFromOne(order, static_cast<float>(EDITOR_WAYPOINT_MAX_ORDER)));
}

uint16_t nextEditorWaypointOrder(const EditorDocument& document,
                                 uint8_t route) {
  uint16_t last = 0;
  for (const EditorWaypoint* waypoint : routeWaypoints(document, route)) {
    last = std::max(last, waypoint->order);
  }
  return clampEditorWaypointOrder(static_cast<float>(last) + 1.0f);
}

EditorWaypoint makeEditorWaypoint(uint8_t route, uint16_t order,
                                  WorldPoint position) {
  EditorWaypoint waypoint;
  waypoint.route = clampEditorRoute(static_cast<float>(route));
  waypoint.order = clampEditorWaypointOrder(static_cast<float>(order));
  waypoint.position = position;
  return waypoint;
}

std::string editorWaypointName(const EditorWaypoint& waypoint) {
  return "Route " + std::to_string(waypoint.route) + " · Waypoint " +
         std::to_string(waypoint.order);
}

float editorWaypointValue(const EditorWaypoint& waypoint,
                          EditorPropertyField field) {
  if (isPositionField(field)) {
    return editorVectorValue(
        waypoint.position,
        editorFieldAxis(field, EditorPropertyField::POSITION_X));
  }
  if (field == EditorPropertyField::ROUTE) {
    return static_cast<float>(waypoint.route);
  }
  return field == EditorPropertyField::ORDER
             ? static_cast<float>(waypoint.order)
             : 0.0f;
}

void setEditorWaypointValue(EditorWaypoint& waypoint, EditorPropertyField field,
                            float value) {
  const float written = normalizeEditorPropertyValue(field, value);
  if (isPositionField(field)) {
    editorVectorAxis(waypoint.position,
                     editorFieldAxis(field, EditorPropertyField::POSITION_X)) =
        written;
  } else if (field == EditorPropertyField::ROUTE) {
    waypoint.route = clampEditorRoute(written);
  } else if (field == EditorPropertyField::ORDER) {
    waypoint.order = clampEditorWaypointOrder(written);
  }
}

bool editorWaypointHasField(EditorPropertyField field) {
  return std::ranges::find(EDITOR_WAYPOINT_FIELDS, field) !=
         std::end(EDITOR_WAYPOINT_FIELDS);
}

PlacementBounds editorWaypointBounds(const EditorWaypoint& waypoint) {
  const WorldPoint& at = waypoint.position;
  const float reach = EDITOR_WAYPOINT_MARKER_RADIUS;
  return {{at.x - reach, at.y - reach, at.z},
          {at.x + reach, at.y + reach, at.z + EDITOR_WAYPOINT_MARKER_HEIGHT}};
}

std::vector<Vec2> editorRoutePoints(const EditorDocument& document,
                                    uint8_t route) {
  std::vector<const EditorWaypoint*> walked = routeWaypoints(document, route);
  std::ranges::stable_sort(walked, {}, &EditorWaypoint::order);
  std::vector<Vec2> points;
  points.reserve(walked.size());
  for (const EditorWaypoint* waypoint : walked) {
    points.push_back({waypoint->position.x, waypoint->position.y});
  }
  return points;
}

std::vector<uint8_t> editorRoutesInUse(const EditorDocument& document) {
  std::vector<uint8_t> routes;
  for (uint8_t route = 1; route <= EDITOR_ROUTE_COUNT; ++route) {
    if (!routeWaypoints(document, route).empty()) {
      routes.push_back(route);
    }
  }
  return routes;
}

EditorRouteChoices editorRouteChoices(const EditorDocument& document,
                                      uint8_t route) {
  EditorRouteChoices choices{.names = {"None"}, .routes = {0}};
  for (const uint8_t used : editorRoutesInUse(document)) {
    choices.names.push_back("Route " + std::to_string(used));
    choices.routes.push_back(used);
  }
  const auto found = std::ranges::find(choices.routes, route);
  if (found == choices.routes.end()) {
    choices.names.push_back("Route " + std::to_string(route) + " (empty)");
    choices.routes.push_back(route);
  }
  choices.current = static_cast<size_t>(
      std::ranges::find(choices.routes, route) - choices.routes.begin());
  return choices;
}

}  // namespace eng::editor
