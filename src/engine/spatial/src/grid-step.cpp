#include <engine/spatial/grid-step.h>

namespace eng::spatial {

bool canGridStep(const NavGrid& grid, GridCell cell, const GridStep& step,
                 uint8_t clearance) {
  if (!grid.isOpen({cell.x + step.dx, cell.y + step.dy}, clearance)) {
    return false;
  }
  if (step.dx == 0 || step.dy == 0) {
    return true;
  }
  return grid.isOpen({cell.x + step.dx, cell.y}, clearance) &&
         grid.isOpen({cell.x, cell.y + step.dy}, clearance);
}

}  // namespace eng::spatial
