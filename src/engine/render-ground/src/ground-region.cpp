#include <algorithm>
#include <engine/render-ground/ground-region.h>

namespace eng {

namespace {

  /// Where @p cell sits in a flag per cell of @p bounds, row by row.
  size_t slotIn(const GroundRect& bounds, GroundCell cell) {
    return static_cast<size_t>(cell.y - bounds.y) *
               static_cast<size_t>(bounds.width) +
           static_cast<size_t>(cell.x - bounds.x);
  }

  /// Whether @p cell lies inside @p bounds.
  bool inside(const GroundRect& bounds, GroundCell cell) {
    return cell.x >= bounds.x && cell.y >= bounds.y &&
           cell.x < bounds.x + bounds.width &&
           cell.y < bounds.y + bounds.height;
  }

  /// A flood in progress: the cells still to look round, and which cells
  /// of the grid it has reached.
  struct Flood {
    /// Cells reached whose neighbours are not looked at yet.
    std::vector<GroundCell> frontier;
    /// One flag per cell of the grid's rectangle, row by row.
    std::vector<bool> reached;
  };

  /// Push every neighbour of @p cell — the eight round it — that holds
  /// @p terrain and @p flood has not reached, marking each reached.
  void spread(const GroundGrid& grid, GroundCell cell, uint8_t terrain,
              Flood& flood) {
    for (int32_t dy = -1; dy <= 1; ++dy) {
      for (int32_t dx = -1; dx <= 1; ++dx) {
        const GroundCell next{cell.x + dx, cell.y + dy};
        if (inside(grid.bounds(), next) && grid.at(next) == terrain &&
            !flood.reached[slotIn(grid.bounds(), next)]) {
          flood.reached[slotIn(grid.bounds(), next)] = true;
          flood.frontier.push_back(next);
        }
      }
    }
  }

  /// @p cells put row by row from the south-west.
  void sortRowMajor(std::vector<GroundCell>& cells) {
    std::ranges::sort(cells, [](GroundCell a, GroundCell b) {
      return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
  }

}  // namespace

std::vector<GroundCell> connectedGroundCells(const GroundGrid& grid,
                                             GroundCell seed) {
  const uint8_t terrain = grid.at(seed);
  if (terrain == 0) {
    return {};
  }
  Flood flood{{seed}, std::vector<bool>(grid.cells().size(), false)};
  flood.reached[slotIn(grid.bounds(), seed)] = true;
  std::vector<GroundCell> region;
  while (!flood.frontier.empty()) {
    const GroundCell cell = flood.frontier.back();
    flood.frontier.pop_back();
    region.push_back(cell);
    spread(grid, cell, terrain, flood);
  }
  sortRowMajor(region);
  return region;
}

}  // namespace eng
