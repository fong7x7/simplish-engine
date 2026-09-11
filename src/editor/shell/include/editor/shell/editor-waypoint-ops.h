#pragma once

/// @file editor-waypoint-ops.h
/// @brief Make, read, and write patrol route waypoints.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstdint>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-placement-bounds.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-route-choices.h>
#include <editor/shell/editor-waypoint.h>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec2.h>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// Routes a level can hold, and so the highest route a waypoint can name.
inline constexpr uint8_t EDITOR_ROUTE_COUNT = 9;

/// The highest place in its route a waypoint can take.
inline constexpr uint16_t EDITOR_WAYPOINT_MAX_ORDER = 99;

/// The definition a waypoint is saved under, in a level file's `entities`
/// list.
inline constexpr std::string_view EDITOR_WAYPOINT_DEFINITION =
    "entity:waypoint";

/// What the browser and the panel call a waypoint.
inline constexpr std::string_view EDITOR_WAYPOINT_NAME = "Waypoint";

/// Half the width of the post a waypoint is drawn and picked as.
inline constexpr float EDITOR_WAYPOINT_MARKER_RADIUS = 0.15f;

/// How tall that post stands, in tiles: knee height, so it reads as a mark
/// on the floor rather than as somebody standing there.
inline constexpr float EDITOR_WAYPOINT_MARKER_HEIGHT = 0.6f;

/// @p route held to a route a level can hold: rounded, then clamped into
/// [1, `EDITOR_ROUTE_COUNT`].
[[nodiscard]] uint8_t clampEditorRoute(float route);

/// @p order held to a place a waypoint can take: rounded, then clamped
/// into [1, `EDITOR_WAYPOINT_MAX_ORDER`].
[[nodiscard]] uint16_t clampEditorWaypointOrder(float order);

/// The place after the last waypoint of @p route in @p document, so
/// dropping waypoints one after another lays a route out in the order they
/// were dropped.
[[nodiscard]] uint16_t nextEditorWaypointOrder(const EditorDocument& document,
                                               uint8_t route);

/// A new waypoint of @p route, at place @p order, standing at @p position.
[[nodiscard]] EditorWaypoint makeEditorWaypoint(uint8_t route, uint16_t order,
                                                WorldPoint position);

/// The name line the panel shows for @p waypoint: `Route 2 · Waypoint 3`.
[[nodiscard]] std::string editorWaypointName(const EditorWaypoint& waypoint);

/// Current value of one of a waypoint's properties; zero for a field it
/// does not have.
[[nodiscard]] float editorWaypointValue(const EditorWaypoint& waypoint,
                                        EditorPropertyField field);

/// Write one of a waypoint's properties, normalised as
/// `normalizeEditorPropertyValue` defines. A field it does not have is
/// ignored.
void setEditorWaypointValue(EditorWaypoint& waypoint, EditorPropertyField field,
                            float value);

/// Whether @p field is one a waypoint has: its route, its place, and its
/// position.
[[nodiscard]] bool editorWaypointHasField(EditorPropertyField field);

/// The post a waypoint is drawn and picked as, standing on its position.
[[nodiscard]] PlacementBounds
editorWaypointBounds(const EditorWaypoint& waypoint);

/// Where route @p route's waypoints stand, in walking order: by `order`,
/// and by the order @p document lists them where two share one.
[[nodiscard]] std::vector<Vec2>
editorRoutePoints(const EditorDocument& document, uint8_t route);

/// Every route @p document has a waypoint of, lowest first.
[[nodiscard]] std::vector<uint8_t>
editorRoutesInUse(const EditorDocument& document);

/// What the Route row offers an actor patrolling @p route: none, then every
/// route in use. A route no waypoint is left in is offered too, last, as
/// `Route N (empty)`, so the row still shows what the prop names.
[[nodiscard]] EditorRouteChoices
editorRouteChoices(const EditorDocument& document, uint8_t route);

}  // namespace eng::editor
