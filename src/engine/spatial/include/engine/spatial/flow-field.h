#pragma once

/// @file flow-field.h
/// @brief How far every cell of a grid is from one goal, for a crowd to
/// walk downhill on.
/// @par Threading
/// Immutable once complete; safe to read from any thread.

#include <cstdint>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <optional>
#include <vector>

namespace eng::spatial {

/// The cost of a cell no path reaches the goal from.
inline constexpr uint32_t FLOW_UNREACHED = UINT32_MAX;

/// An integration field (Game REQUIREMENTS §5.2): for one goal and one
/// clearance, the cost of the cheapest walk from every cell of a grid to
/// the goal, in the same integer steps A* takes — 10 along an axis, 14
/// diagonally, never past a corner. One search serves every character that
/// shares the goal: each looks at the cells beside it and steps to the one
/// that is cheapest to go on from, so a horde of two thousand pursuing one
/// player costs one search, not two thousand.
///
/// The costs are shortest-path distances, so they are unique: however a
/// `FlowFieldBuilder` split the work between ticks, the finished field is
/// the same, on every machine.
class FlowField {
public:
  /// A field with no cells, reaching nothing.
  FlowField() = default;

  /// Start over as a field of @p cells cells toward @p goal for characters
  /// needing @p clearance, with every cell unreached.
  void reset(uint32_t cells, GridCell goal, uint8_t clearance);
  /// Set the cost of the cell at row-major @p index.
  void setCost(uint32_t index, uint32_t cost) { cost_[index] = cost; }
  /// Mark every cell's cost final.
  void markComplete() { complete_ = true; }

  /// Whether the costs are final.
  [[nodiscard]] bool complete() const { return complete_; }
  /// The cell every walk down the field ends at.
  [[nodiscard]] GridCell goal() const { return goal_; }
  /// The clearance the field was built for.
  [[nodiscard]] uint8_t clearance() const { return clearance_; }
  /// The cost from the cell at row-major @p index; `FLOW_UNREACHED` for
  /// one no walk joins to the goal, and for an index past the field.
  [[nodiscard]] uint32_t cost(uint32_t index) const;

  /// The cell @p steps steps down the field from @p from on @p grid — each
  /// step to the neighbour a walk from which to the goal is cheapest, the
  /// first in `GRID_STEPS` order among equals — stopping early at the goal.
  /// Nothing when @p from is off the grid or unreached.
  [[nodiscard]] std::optional<GridCell>
  descend(const NavGrid& grid, GridCell from, uint32_t steps) const;

private:
  /// The neighbour of @p cell one step further down the field, or @p cell
  /// itself when none is cheaper to go on from.
  [[nodiscard]] GridCell downhill(const NavGrid& grid, GridCell cell) const;

  /// Each cell's cost, row-major.
  std::vector<uint32_t> cost_;
  /// The goal.
  GridCell goal_{};
  /// The clearance the field was built for.
  uint8_t clearance_ = 1;
  /// Whether the costs are final.
  bool complete_ = false;
};

}  // namespace eng::spatial
