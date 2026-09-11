#pragma once

/// @file path-result.h
/// @brief What a path search gives back.
/// @par Threading
/// A value type viewing a `PathFinder`'s buffer.

#include <cstdint>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/path-status.h>
#include <span>

namespace eng::spatial {

/// The outcome of one search. `cells` views the finder's own buffer, so it
/// is valid until that finder searches again.
struct PathResult {
  /// How the search ended.
  PathStatus status = PathStatus::UNREACHABLE;
  /// Cells the search expanded: its cost, for budgeting a tick.
  uint32_t expanded = 0;
  /// The path's length in cost units — 10 per straight step, 14 per
  /// diagonal. 0 unless `status` is `FOUND`.
  uint32_t cost = 0;
  /// Every cell of the path, start and goal included, in walking order.
  /// Empty unless `status` is `FOUND`.
  std::span<const GridCell> cells{};
};

}  // namespace eng::spatial
