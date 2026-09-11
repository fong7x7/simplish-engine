#include <cmath>
#include <cstdlib>
#include <engine/spatial/line-of-sight.h>
#include <limits>

namespace eng::spatial {

namespace {

  /// Furthest a point may be from the grid's origin, in cells, and still be
  /// walked: well past any grid, and far short of overflowing an int32.
  constexpr float WALK_LIMIT_CELLS = 1.0e7F;

  /// Where a walk is along one axis.
  struct AxisWalk {
    /// Which way a step goes: -1, 0 or +1.
    int32_t step = 0;
    /// How far along the segment, as a fraction, the next boundary is.
    float next = std::numeric_limits<float>::infinity();
    /// How much of the segment one whole cell takes.
    float delta = std::numeric_limits<float>::infinity();
  };

  /// Where a segment's walk across the grid has got to.
  struct CellWalk {
    /// The cell the walk is in.
    GridCell cell{};
    /// The cell the segment ends in.
    GridCell end{};
    /// Progress along X.
    AxisWalk x{};
    /// Progress along Y.
    AxisWalk y{};
  };

  /// Whether @p cell lets the walk through.
  bool passes(const NavGrid& grid, GridCell cell, uint8_t clearance) {
    return !grid.contains(cell) || grid.isOpen(cell, clearance);
  }

  /// @p world in cells from the grid's origin, along one axis.
  float inCells(float world, float origin, float cell_size) {
    return (world - origin) / cell_size;
  }

  /// The walk along one axis from @p a to @p b, in cells: which way it
  /// steps, how far to the first boundary, and how far between them.
  AxisWalk beginAxis(float a, float b) {
    AxisWalk axis;
    const float d = b - a;
    if (d == 0.0F) {
      return axis;
    }
    axis.step = d > 0.0F ? 1 : -1;
    axis.delta = std::fabs(1.0F / d);
    const float floor_a = std::floor(a);
    axis.next = (axis.step > 0 ? floor_a + 1.0F - a : a - floor_a) * axis.delta;
    return axis;
  }

  /// Step through a corner diagonally. False when either cell beside the
  /// corner refuses the walk.
  bool advanceCorner(CellWalk& walk, const NavGrid& grid, uint8_t clearance) {
    const GridCell beside_x{walk.cell.x + walk.x.step, walk.cell.y};
    const GridCell beside_y{walk.cell.x, walk.cell.y + walk.y.step};
    if (!passes(grid, beside_x, clearance) ||
        !passes(grid, beside_y, clearance)) {
      return false;
    }
    walk.cell = {beside_x.x, beside_y.y};
    walk.x.next += walk.x.delta;
    walk.y.next += walk.y.delta;
    return true;
  }

  /// Advance the walk one cell — diagonally, at a corner. False when a
  /// corner is squeezed between two cells that do not pass.
  bool advance(CellWalk& walk, const NavGrid& grid, uint8_t clearance) {
    if (walk.x.next == walk.y.next) {
      return advanceCorner(walk, grid, clearance);
    }
    if (walk.x.next < walk.y.next) {
      walk.cell.x += walk.x.step;
      walk.x.next += walk.x.delta;
    } else {
      walk.cell.y += walk.y.step;
      walk.y.next += walk.y.delta;
    }
    return true;
  }

  /// Whether @p a, in cells, is near enough the grid to walk from.
  bool walkable(Vec2 a) {
    return std::fabs(a.x) < WALK_LIMIT_CELLS &&
           std::fabs(a.y) < WALK_LIMIT_CELLS;
  }

  /// A walk from @p a to @p b, both in cells from the grid's origin.
  CellWalk beginWalk(Vec2 a, Vec2 b) {
    return {.cell = {static_cast<int32_t>(std::floor(a.x)),
                     static_cast<int32_t>(std::floor(a.y))},
            .end = {static_cast<int32_t>(std::floor(b.x)),
                    static_cast<int32_t>(std::floor(b.y))},
            .x = beginAxis(a.x, b.x),
            .y = beginAxis(a.y, b.y)};
  }

  /// Walk @p walk to its end cell, or until a cell refuses it.
  bool walkToEnd(CellWalk walk, const NavGrid& grid, uint8_t clearance) {
    // Exactly this many steps reach the end; more means a rounding lost it.
    int64_t steps = std::abs(static_cast<int64_t>(walk.end.x) - walk.cell.x) +
                    std::abs(static_cast<int64_t>(walk.end.y) - walk.cell.y);
    while (passes(grid, walk.cell, clearance)) {
      if (walk.cell == walk.end) {
        return true;
      }
      if (steps-- <= 0 || !advance(walk, grid, clearance)) {
        return false;
      }
    }
    return false;
  }

}  // namespace

bool hasLineOfSight(const NavGrid& grid, Vec2 from, Vec2 to,
                    uint8_t clearance) {
  if (grid.cellCount() == 0) {
    return true;
  }
  const NavGridSpec& spec = grid.spec();
  const Vec2 a{inCells(from.x, spec.origin.x, spec.cell_size),
               inCells(from.y, spec.origin.y, spec.cell_size)};
  const Vec2 b{inCells(to.x, spec.origin.x, spec.cell_size),
               inCells(to.y, spec.origin.y, spec.cell_size)};
  if (!walkable(a) || !walkable(b)) {
    return false;
  }
  return walkToEnd(beginWalk(a, b), grid, clearance);
}

}  // namespace eng::spatial
