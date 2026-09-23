#pragma once

/// @file ground-region.h
/// @brief The painted area a cell belongs to.
/// @par Threading Thread-safe (pure function over value types).

#include <engine/render-ground/ground-cell.h>
#include <engine/render-ground/ground-grid.h>
#include <vector>

namespace eng {

/// Every cell of @p grid joined to @p seed through cells of the same
/// terrain, @p seed among them, row by row from the south-west: the patch
/// of sand, or the length of road, that @p seed is part of.
///
/// Cells touching only at a corner count as joined, since that is how
/// `groundQuarterShape` draws them — a diagonal road is one road on screen,
/// and one here. Empty when @p seed is bare: bare ground is everything
/// unpainted, not an area of its own.
[[nodiscard]] std::vector<GroundCell>
connectedGroundCells(const GroundGrid& grid, GroundCell seed);

}  // namespace eng
