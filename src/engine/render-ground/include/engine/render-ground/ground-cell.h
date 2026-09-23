#pragma once

/// @file ground-cell.h
/// @brief One cell of the ground grid, by its integer coordinates.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng {

/// A cell of the ground grid: the tile whose south-west corner is at world
/// (`x`, `y`), so it covers x to x + 1 and y to y + 1.
///
/// Integers rather than a world point, because a cell is a slot in a grid
/// and a painted cell has to be the same one on every machine: the float
/// a cursor happens to land on is floored to one of these once, at the
/// edge, and nothing after that does arithmetic on it in floats.
/// @thread_safety Immutable value type.
struct GroundCell {
  /// World X of the cell's west edge, in tiles.
  int32_t x = 0;
  /// World Y of the cell's south edge, in tiles.
  int32_t y = 0;

  /// Two cells are the same cell when both coordinates agree.
  bool operator==(const GroundCell&) const = default;
};

}  // namespace eng
