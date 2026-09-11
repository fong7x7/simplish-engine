#pragma once

/// @file nearest-open-cell.h
/// @brief The nearest cell a character of a given size can stand in.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <optional>

namespace eng::spatial {

/// The open cell nearest @p around with at least @p clearance, searching no
/// more than @p max_rings cells out; nothing when there is none that near.
///
/// What a planner asks when a goal is somewhere a character cannot stand —
/// a player backed against a crate, a spot picked at random that landed on
/// a prop. Rings are searched outward by chessboard distance; within the
/// nearest ring holding an open cell, the one nearest in straight-line
/// distance wins, and a tie goes to the first in row-major order.
[[nodiscard]] std::optional<GridCell> nearestOpenCell(const NavGrid& grid,
                                                      GridCell around,
                                                      uint8_t clearance,
                                                      uint32_t max_rings);

}  // namespace eng::spatial
