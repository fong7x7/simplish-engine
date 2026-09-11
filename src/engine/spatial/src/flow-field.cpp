#include <engine/spatial/flow-field.h>
#include <engine/spatial/grid-step.h>

namespace eng::spatial {

void FlowField::reset(uint32_t cells, GridCell goal, uint8_t clearance) {
  cost_.assign(cells, FLOW_UNREACHED);
  goal_ = goal;
  clearance_ = clearance;
  complete_ = false;
}

uint32_t FlowField::cost(uint32_t index) const {
  return index < cost_.size() ? cost_[index] : FLOW_UNREACHED;
}

GridCell FlowField::downhill(const NavGrid& grid, GridCell cell) const {
  const uint32_t here = cost(grid.indexOf(cell));
  GridCell best = cell;
  uint32_t best_total = FLOW_UNREACHED;
  for (const GridStep& step : GRID_STEPS) {
    if (!canGridStep(grid, cell, step, clearance_)) {
      continue;
    }
    const GridCell next{cell.x + step.dx, cell.y + step.dy};
    const uint32_t there = cost(grid.indexOf(next));
    if (there < here && there + step.cost < best_total) {
      best = next;
      best_total = there + step.cost;
    }
  }
  return best;
}

std::optional<GridCell> FlowField::descend(const NavGrid& grid, GridCell from,
                                           uint32_t steps) const {
  if (!grid.contains(from) || cost(grid.indexOf(from)) == FLOW_UNREACHED) {
    return std::nullopt;
  }
  GridCell at = from;
  for (uint32_t i = 0; i < steps && !(at == goal_); ++i) {
    const GridCell next = downhill(grid, at);
    if (next == at) {
      break;
    }
    at = next;
  }
  return at;
}

}  // namespace eng::spatial
