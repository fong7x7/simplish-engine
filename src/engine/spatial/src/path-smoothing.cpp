#include <engine/spatial/line-of-sight.h>
#include <engine/spatial/path-smoothing.h>

namespace eng::spatial {

namespace {

  /// The furthest cell after @p anchor in @p cells that a straight walk
  /// from @p anchor reaches, stopping at the first one it does not.
  size_t furthestReached(const NavGrid& grid, std::span<const GridCell> cells,
                         size_t anchor, uint8_t clearance) {
    const Vec2 from = grid.centre(cells[anchor]);
    size_t reach = anchor + 1;
    while (
        reach + 1 < cells.size() &&
        hasLineOfSight(grid, from, grid.centre(cells[reach + 1]), clearance)) {
      ++reach;
    }
    return reach;
  }

}  // namespace

size_t smoothPath(const NavGrid& grid, std::span<const GridCell> cells,
                  uint8_t clearance, std::span<Vec2> out) {
  size_t count = 0;
  size_t anchor = 0;
  while (anchor + 1 < cells.size() && count < out.size()) {
    anchor = furthestReached(grid, cells, anchor, clearance);
    out[count++] = grid.centre(cells[anchor]);
  }
  return count;
}

}  // namespace eng::spatial
