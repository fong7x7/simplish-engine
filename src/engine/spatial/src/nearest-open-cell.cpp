#include <algorithm>
#include <cstdlib>
#include <engine/spatial/nearest-open-cell.h>
#include <limits>

namespace eng::spatial {

namespace {

  /// The open cell on the ring @p ring cells out from @p around nearest it
  /// in straight-line distance, or nothing when the ring holds none.
  std::optional<GridCell> bestOnRing(const NavGrid& grid, GridCell around,
                                     uint8_t clearance, int32_t ring) {
    std::optional<GridCell> best;
    int32_t best_distance = std::numeric_limits<int32_t>::max();
    for (int32_t dy = -ring; dy <= ring; ++dy) {
      for (int32_t dx = -ring; dx <= ring; ++dx) {
        const GridCell cell{around.x + dx, around.y + dy};
        const int32_t distance = dx * dx + dy * dy;
        if (std::max(std::abs(dx), std::abs(dy)) == ring &&
            distance < best_distance && grid.isOpen(cell, clearance)) {
          best = cell;
          best_distance = distance;
        }
      }
    }
    return best;
  }

}  // namespace

std::optional<GridCell> nearestOpenCell(const NavGrid& grid, GridCell around,
                                        uint8_t clearance, uint32_t max_rings) {
  for (uint32_t ring = 0; ring <= max_rings; ++ring) {
    const std::optional<GridCell> best =
        bestOnRing(grid, around, clearance, static_cast<int32_t>(ring));
    if (best) {
      return best;
    }
  }
  return std::nullopt;
}

}  // namespace eng::spatial
