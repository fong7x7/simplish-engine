#pragma once

/// @file nav-grid-fit.h
/// @brief Choosing the rectangle a navigation grid covers.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/physics/collision-box.h>
#include <engine/spatial/nav-grid-spec.h>
#include <span>

namespace eng::spatial {

/// How far past everything it is fitted to a grid reaches, in tiles: room
/// for a character to walk round the outside of the outermost prop, and to
/// flee a little way from the level before it runs out of floor.
inline constexpr float NAV_GRID_MARGIN_TILES = 8.0F;

/// The most cells a fitted grid has along either side: 256 tiles at a
/// quarter-tile cell. A level larger than that is cut at its far edges.
inline constexpr uint32_t NAV_GRID_MAX_SIDE_CELLS = 1024;

/// A grid covering the footprints of @p boxes and every point of
/// @p points, with `NAV_GRID_MARGIN_TILES` to spare on every side, laid on
/// the floor at @p floor_z.
///
/// Until the level has tile layers with bounds of their own, this is what
/// says how big the level is: whatever is in it, plus the margin. The
/// origin is snapped down to a whole tile, so tile-aligned props fill whole
/// cells exactly and the grid does not shift by a fraction of a cell when
/// one prop moves. Nothing to fit gives a grid with no cells.
[[nodiscard]] NavGridSpec
fitNavGrid(std::span<const physics::CollisionBox> boxes,
           std::span<const Vec2> points, float floor_z);

}  // namespace eng::spatial
