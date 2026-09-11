#include <engine/spatial/grid-step.h>
#include <engine/spatial/reachability.h>

namespace eng::spatial {

namespace {

  /// A flood fill in progress.
  struct Flood {
    /// The grid being filled.
    const NavGrid& grid;
    /// The clearance every reached cell has.
    uint8_t clearance = 1;
    /// Each cell: 1 once reached.
    std::vector<uint8_t> reached;
    /// Every cell reached, in the order it was; the fill walks it.
    std::vector<GridCell> queue;
  };

  /// Mark @p cell reached and queue it, when it is open and not reached yet.
  void reach(Flood& flood, GridCell cell) {
    if (!flood.grid.isOpen(cell, flood.clearance) ||
        flood.reached[flood.grid.indexOf(cell)] != 0) {
      return;
    }
    flood.reached[flood.grid.indexOf(cell)] = 1;
    flood.queue.push_back(cell);
  }

  /// Reach every neighbour a step from @p cell may go to.
  void spread(Flood& flood, GridCell cell) {
    for (const GridStep& step : GRID_STEPS) {
      if (canGridStep(flood.grid, cell, step, flood.clearance)) {
        reach(flood, {cell.x + step.dx, cell.y + step.dy});
      }
    }
  }

}  // namespace

std::vector<uint8_t> reachableCells(const NavGrid& grid,
                                    std::span<const GridCell> sources,
                                    uint8_t clearance) {
  Flood flood{grid, clearance, std::vector<uint8_t>(grid.cellCount(), 0), {}};
  for (const GridCell source : sources) {
    reach(flood, source);
  }
  // Indexed, not iterated: `spread` grows the queue as it walks it.
  for (size_t next = 0; next < flood.queue.size(); ++next) {
    spread(flood, flood.queue[next]);
  }
  return flood.reached;
}

}  // namespace eng::spatial
