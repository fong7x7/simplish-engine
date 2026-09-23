#pragma once

/// @file ground-grid.h
/// @brief The terrain painted on each cell of a level's floor.
/// @par Threading Main-thread-only (owns heap storage).

#include <cstdint>
#include <engine/render-ground/ground-cell.h>
#include <engine/render-ground/ground-rect.h>
#include <optional>
#include <vector>

namespace eng {

/// How far from the origin, in cells, the ground may be painted: the grid
/// holds cells from −limit to limit − 1 on each axis.
///
/// A level is a hundred-odd tiles across, so this is far past anything
/// authored; it exists so a mistyped coordinate cannot ask for gigabytes.
inline constexpr int32_t GROUND_COORDINATE_LIMIT = 2048;

/// The size, in cells, the grid's rectangle grows in: it is always a whole
/// number of these blocks, so painting a stroke outward grows it a block at
/// a time rather than reallocating for every cell.
inline constexpr int32_t GROUND_GROWTH_BLOCK = 16;

/// A rectangle of cells, each holding a terrain number.
///
/// Terrain 0 is bare ground — nothing painted — and is what every cell
/// outside the rectangle holds, so the grid can start empty and grow to
/// cover whatever is painted without a size being chosen up front. The
/// numbers mean nothing here beyond their order: which terrain each is,
/// and what it looks like, is the caller's palette (`ground-mesh.h` stacks
/// them in that order).
///
/// Stored row by row from the south-west, which is the order a level file's
/// runs are written in (`ground-runs.h`).
/// @thread_safety Main-thread-only.
class GroundGrid {
public:
  /// An empty grid: every cell bare.
  GroundGrid() = default;

  /// A grid covering @p bounds with @p cells, row by row from the south-west.
  /// Nothing when the count does not match the rectangle, or when the
  /// rectangle reaches past `GROUND_COORDINATE_LIMIT`.
  [[nodiscard]] static std::optional<GroundGrid>
  fromCells(GroundRect bounds, std::vector<uint8_t> cells);

  /// The terrain at @p cell, 0 outside the rectangle.
  [[nodiscard]] uint8_t at(GroundCell cell) const;

  /// Paint @p cell with @p terrain, growing the rectangle to reach it. False
  /// when nothing changed: the cell already held it, or lies past
  /// `GROUND_COORDINATE_LIMIT`.
  bool set(GroundCell cell, uint8_t terrain);

  /// The rectangle the grid holds cells for. Every cell outside it is bare,
  /// though cells inside it may be bare too.
  [[nodiscard]] GroundRect bounds() const { return bounds_; }

  /// The cells, row by row from the south-west of `bounds()`.
  [[nodiscard]] const std::vector<uint8_t>& cells() const { return cells_; }

  /// The smallest rectangle holding every painted cell, empty when there is
  /// none.
  [[nodiscard]] GroundRect paintedBounds() const;

  /// Whether no cell is painted.
  [[nodiscard]] bool empty() const;

  /// Two grids are equal when they hold the same rectangle of the same
  /// cells. Two grids painting the same cells over different rectangles are
  /// not, which is what a caller comparing file contents wants.
  bool operator==(const GroundGrid&) const = default;

private:
  /// Where @p cell sits in `cells_`, or nothing when it is outside.
  [[nodiscard]] std::optional<size_t> slotOf(GroundCell cell) const;

  /// Widen the rectangle to hold @p cell, keeping every cell's terrain.
  void growToInclude(GroundCell cell);

  /// The cells the grid holds.
  GroundRect bounds_{};
  /// One terrain number per cell of `bounds_`.
  std::vector<uint8_t> cells_{};
};

}  // namespace eng
