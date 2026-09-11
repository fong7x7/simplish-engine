#pragma once

/// @file path-smoothing.h
/// @brief Turning a path of grid cells into a few straight legs.
/// @par Threading
/// Pure functions.

#include <cstddef>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <span>

namespace eng::spatial {

/// The waypoints a character walking @p cells should head for, written to
/// @p out; how many were written.
///
/// A path of cells steps in eight directions, so walked cell by cell it
/// zigzags. Smoothed, it is a few straight legs: from each waypoint, the
/// next is the centre of the furthest cell along the path that a straight
/// walk with @p clearance still reaches (`hasLineOfSight`). The start cell
/// is not a waypoint — the character is already there — and the last is
/// the goal cell's centre, unless @p out fills first, in which case the
/// caller walks what it has and asks again from there.
///
/// Scans forward from each waypoint and stops at the first cell a straight
/// walk cannot reach, so it is linear in the path's length. Deterministic
/// as the line walk is.
[[nodiscard]] size_t smoothPath(const NavGrid& grid,
                                std::span<const GridCell> cells,
                                uint8_t clearance, std::span<Vec2> out);

}  // namespace eng::spatial
