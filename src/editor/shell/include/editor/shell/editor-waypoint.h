#pragma once

/// @file editor-waypoint.h
/// @brief One point of a patrol route.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/iso-projection.h>
#include <string>

namespace eng::editor {

/// A point of a patrol route: the waypoints of one route, walked in order
/// of `order`, are the round a prop with a `patrol` behavior walks when
/// its Route row names that route
/// ([actors.md](../../../../../docs/game/actors.md)).
///
/// Routes are numbered rather than named — the properties panel steps a
/// number but has nowhere to type a name — and a level holds up to
/// `EDITOR_ROUTE_COUNT` of them. Saved with the level as an entity of
/// definition `entity:waypoint` ([project-format.md §4.1]).
/// @thread_safety Main-thread-only.
struct EditorWaypoint {
  /// Stable identifier for this one waypoint: `waypoint_01`. Assigned when
  /// it is added and never reused — see `editor-entity-id.h`.
  std::string id{};
  /// Which route it belongs to, 1 to `EDITOR_ROUTE_COUNT`.
  uint8_t route = 1;
  /// Where it comes in its route: lower first. Two waypoints of one route
  /// with the same order are walked in the order the level lists them.
  uint16_t order = 1;
  /// Where it stands: the centre of the tile it was dropped on.
  WorldPoint position{};
};

}  // namespace eng::editor
