#pragma once

/// @file grid-cell.h
/// @brief One cell of a navigation grid, by column and row.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::spatial {

/// A cell of a `NavGrid`: column `x` and row `y`, counted from the grid's
/// origin corner. Signed, so a neighbour one step off the grid's edge is a
/// cell that exists and is simply not on the grid.
struct GridCell {
  /// Column, counted along world +X.
  int32_t x = 0;
  /// Row, counted along world +Y.
  int32_t y = 0;

  /// Cells are equal when both coordinates are.
  bool operator==(const GridCell&) const = default;
};

}  // namespace eng::spatial
