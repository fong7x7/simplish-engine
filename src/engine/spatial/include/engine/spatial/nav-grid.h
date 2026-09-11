#pragma once

/// @file nav-grid.h
/// @brief Where characters of a given size can stand, as a grid of cells.
/// @par Threading
/// Immutable after construction; safe to read from any thread.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/physics/collision-box.h>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid-spec.h>
#include <optional>
#include <span>
#include <vector>

namespace eng::spatial {

/// The level's floor as a grid of square cells, each knowing how far it is
/// from the nearest solid one (Engine REQUIREMENTS §6, `spatial`).
///
/// Built from the level's solid boxes, the same `physics::CollisionBox` list
/// the tick collides against, so what a planner routes around is exactly
/// what movement stops against. A cell is *solid* when any box overlaps it
/// within the height band the spec names — conservatively, so a box edge
/// anywhere inside a cell makes the whole cell solid.
///
/// Every cell carries a **clearance**: its chessboard distance to the
/// nearest solid cell, 0 for a solid cell, saturating at 255. One number
/// per cell serves characters of every size at once: a character fits in a
/// cell when the cell's clearance is at least `requiredClearance(radius)`.
/// Sight asks for clearance 1 — any cell that is not itself solid — so a
/// character can see through a gap it cannot fit through.
///
/// Derived, never authored: nothing here is saved in a level file
/// (project-format §11). It is a pure function of its spec and boxes, so
/// every peer that builds one from the same setup holds the same grid.
class NavGrid {
public:
  /// An empty grid: no cells, so every point is off it.
  NavGrid() = default;

  /// The grid @p spec describes, with the cells @p boxes overlap solid.
  NavGrid(const NavGridSpec& spec,
          std::span<const physics::CollisionBox> boxes);

  /// The spec the grid was built from.
  [[nodiscard]] const NavGridSpec& spec() const { return spec_; }
  /// Cells in the grid.
  [[nodiscard]] uint32_t cellCount() const;

  /// Whether @p cell is on the grid.
  [[nodiscard]] bool contains(GridCell cell) const;
  /// The cell @p point lies in, or nothing when it is off the grid.
  [[nodiscard]] std::optional<GridCell> cellAt(Vec2 point) const;
  /// World X and Y of @p cell's centre.
  [[nodiscard]] Vec2 centre(GridCell cell) const;
  /// @p cell's position in row-major order. @p cell must be on the grid.
  [[nodiscard]] uint32_t indexOf(GridCell cell) const;
  /// The cell at row-major position @p index.
  [[nodiscard]] GridCell cellOf(uint32_t index) const;

  /// @p cell's clearance; 0 for a cell off the grid.
  [[nodiscard]] uint8_t clearance(GridCell cell) const;
  /// Whether @p cell is on the grid with at least @p required clearance.
  [[nodiscard]] bool isOpen(GridCell cell, uint8_t required) const;
  /// The clearance a character of @p radius tiles needs to stand in a cell:
  /// enough that the nearest solid cell's nearest edge is at least
  /// @p radius from the cell's centre. At least 1.
  [[nodiscard]] uint8_t requiredClearance(float radius) const;

private:
  /// Mark every cell a box overlaps as solid.
  void rasterize(std::span<const physics::CollisionBox> boxes);
  /// Turn the solid marks into chessboard distances, in two sweeps.
  void computeClearance();
  /// The smallest of a cell's clearance and one more than each neighbour
  /// in @p neighbours (offsets from it) that is on the grid.
  [[nodiscard]] uint8_t relaxed(GridCell cell,
                                std::span<const GridCell> neighbours) const;

  /// What the grid covers and how it was built.
  NavGridSpec spec_{};
  /// Each cell's clearance, row-major.
  std::vector<uint8_t> clearance_;
};

}  // namespace eng::spatial
