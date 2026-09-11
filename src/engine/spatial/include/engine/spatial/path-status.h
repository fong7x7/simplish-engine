#pragma once

/// @file path-status.h
/// @brief How a path search ended.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::spatial {

/// How a `PathFinder::find` ended. Only `FOUND` carries a path.
enum class PathStatus : uint8_t {
  /// A shortest path was found.
  FOUND,
  /// Every cell reachable from the start was searched; the goal is not one.
  UNREACHABLE,
  /// The search spent its expansion budget before reaching the goal. The
  /// goal may still be reachable; asking again with more budget may say so.
  OVER_BUDGET,
  /// The start or the goal is off the grid or too narrow for the clearance.
  BLOCKED_ENDPOINT,
};

}  // namespace eng::spatial
