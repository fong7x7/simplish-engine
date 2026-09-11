#pragma once

/// @file nav-grid-spec.h
/// @brief Where a navigation grid lies, how fine it is, and what blocks it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec2.h>

namespace eng::spatial {

/// The side of one navigation cell, in tiles. A quarter tile, so a gap one
/// tile wide between two props — which a player a little narrower than a
/// tile walks through — has two cells down its middle clear enough for a
/// character of the same size.
inline constexpr float NAV_CELL_SIZE_TILES = 0.25F;

/// How tall a character a grid is for, in tiles: a box wholly above this
/// height is walked under, as it is by a player.
inline constexpr float NAV_CLEAR_HEIGHT_TILES = 1.5F;

/// The rectangle of world a `NavGrid` covers and the floor it is laid on.
struct NavGridSpec {
  /// World X and Y of the corner of cell (0, 0) with the smallest X and Y.
  Vec2 origin{};
  /// Columns, along world +X.
  uint32_t width = 0;
  /// Rows, along world +Y.
  uint32_t height = 0;
  /// The side of one cell, in tiles.
  float cell_size = NAV_CELL_SIZE_TILES;
  /// World Z of the floor characters stand on.
  float floor_z = 0.0F;
  /// Height above the floor a box must reach into to block a cell.
  float clear_height = NAV_CLEAR_HEIGHT_TILES;
};

}  // namespace eng::spatial
