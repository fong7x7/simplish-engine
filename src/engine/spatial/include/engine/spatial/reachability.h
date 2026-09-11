#pragma once

/// @file reachability.h
/// @brief Every cell a character can walk to from somewhere.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <span>
#include <vector>

namespace eng::spatial {

/// For every cell of @p grid, row-major, 1 when a character needing
/// @p clearance can walk to it from one of @p sources, by the steps A*
/// takes; 0 otherwise. A source that is not open reaches nothing.
///
/// A flood fill, not a search: it answers "is there any way" for every cell
/// at once, which is what an editor flags as unreachable and what a path
/// request to one of those cells would find. It allocates its result, so it
/// is for tools and load-time checks rather than for a tick.
[[nodiscard]] std::vector<uint8_t>
reachableCells(const NavGrid& grid, std::span<const GridCell> sources,
               uint8_t clearance);

}  // namespace eng::spatial
