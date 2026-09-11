#pragma once

/// @file editor-path-answer.h
/// @brief The route an actor would take between two points of a level.
/// @par Threading Thread-safe (value type).

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/spatial/path-status.h>
#include <vector>

namespace eng::editor {

/// What planning a path across a level's navigation grid found.
/// @thread_safety Value type.
struct EditorPathAnswer {
  /// How the search ended.
  spatial::PathStatus status = spatial::PathStatus::UNREACHABLE;
  /// The smoothed waypoints after the start, in walking order; empty unless
  /// a path was found. A straight walk is one waypoint: the goal.
  std::vector<Vec2> waypoints{};
  /// How far walking them is, from the start, in tiles.
  float length_tiles = 0.0F;
  /// Cells the search expanded.
  uint32_t expanded = 0;
};

}  // namespace eng::editor
