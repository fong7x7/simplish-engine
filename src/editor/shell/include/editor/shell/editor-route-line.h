#pragma once

/// @file editor-route-line.h
/// @brief One leg of a patrol route, as the viewport draws it.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <editor/shell/iso-projection.h>

namespace eng::editor {

/// A line from one waypoint of a route to the next in walking order.
/// @thread_safety Immutable value type.
struct EditorRouteLine {
  /// The waypoint the leg starts at.
  WorldPoint from{};
  /// The waypoint it ends at.
  WorldPoint to{};
  /// The route it belongs to, which picks its colour.
  uint8_t route = 1;
};

}  // namespace eng::editor
