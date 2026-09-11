#pragma once

/// @file grid-step.h
/// @brief The eight steps a path may take from a cell, and when it may.
/// @par Threading
/// Constants and pure functions.

#include <array>
#include <cstdint>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>

namespace eng::spatial {

/// Cost of a step along an axis.
inline constexpr uint32_t GRID_STRAIGHT_COST = 10;

/// Cost of a diagonal step: √2 × 10, rounded, which keeps the octile
/// heuristic consistent so A* never closes a cell twice.
inline constexpr uint32_t GRID_DIAGONAL_COST = 14;

/// One of the eight steps from a cell to a neighbour.
struct GridStep {
  /// Columns moved.
  int32_t dx = 0;
  /// Rows moved.
  int32_t dy = 0;
  /// What the step costs.
  uint32_t cost = 0;
};

/// The steps, in the order neighbours are visited: the four along the axes,
/// then the four diagonals. The order is part of the determinism contract —
/// it decides which of two equal paths A* finds.
inline constexpr std::array<GridStep, 8> GRID_STEPS{
    {{1, 0, GRID_STRAIGHT_COST},
     {-1, 0, GRID_STRAIGHT_COST},
     {0, 1, GRID_STRAIGHT_COST},
     {0, -1, GRID_STRAIGHT_COST},
     {1, 1, GRID_DIAGONAL_COST},
     {-1, 1, GRID_DIAGONAL_COST},
     {1, -1, GRID_DIAGONAL_COST},
     {-1, -1, GRID_DIAGONAL_COST}}};

/// Whether a character needing @p clearance may step from @p cell by
/// @p step: onto an open cell and, for a diagonal, past two open cells
/// beside it — so it never clips the corner it rounds.
[[nodiscard]] bool canGridStep(const NavGrid& grid, GridCell cell,
                               const GridStep& step, uint8_t clearance);

}  // namespace eng::spatial
