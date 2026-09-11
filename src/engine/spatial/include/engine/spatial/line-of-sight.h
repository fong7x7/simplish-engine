#pragma once

/// @file line-of-sight.h
/// @brief Whether a straight line crosses only open cells of a grid.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/spatial/nav-grid.h>

namespace eng::spatial {

/// Whether every cell the segment from @p from to @p to passes through has
/// at least @p clearance — so whether a character can see along it
/// (clearance 1: nothing solid in the way) or walk along it (the clearance
/// its radius needs: nothing solid near enough to brush).
///
/// Walks exactly the cells the segment crosses, in order, stepping to
/// whichever cell boundary it meets first. Where it passes exactly through
/// a corner, both cells beside the corner must pass too, so a line cannot
/// slip between two solid cells that touch diagonally — the same rule the
/// planner's diagonal steps follow.
///
/// Cells off the grid pass: the grid covers everything solid with a margin
/// to spare, so off it there is nothing to be in the way. A grid with no
/// cells therefore blocks nothing.
[[nodiscard]] bool hasLineOfSight(const NavGrid& grid, Vec2 from, Vec2 to,
                                  uint8_t clearance);

}  // namespace eng::spatial
