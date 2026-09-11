#pragma once

/// @file path-request.h
/// @brief What a path is asked for: between which cells, for how wide a
/// character, and for how much work at most.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/spatial/grid-cell.h>

namespace eng::spatial {

/// Most cells one search visits unless it is asked for fewer — enough to
/// cross a large level through a maze, and a bound on what one request can
/// cost a tick.
inline constexpr uint32_t PATH_DEFAULT_MAX_EXPANSIONS = 16384;

/// One request to a `PathFinder`.
struct PathRequest {
  /// The cell the path starts in.
  GridCell from{};
  /// The cell the path should reach.
  GridCell to{};
  /// The clearance every cell of the path must have: the character's
  /// `NavGrid::requiredClearance`.
  uint8_t clearance = 1;
  /// Most cells the search may expand before it gives up.
  uint32_t max_expansions = PATH_DEFAULT_MAX_EXPANSIONS;
};

}  // namespace eng::spatial
