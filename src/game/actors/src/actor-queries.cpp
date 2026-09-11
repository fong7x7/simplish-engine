#include "actor-queries.h"

#include "actor-tuning.h"

#include <engine/spatial/nearest-open-cell.h>

namespace eng::game {

Vec2 flat(Vec3 v) {
  return {v.x, v.y};
}

const ActorBrain& brainOf(const ActorRef& a, const ActorTickContext& context) {
  return context.brains[a.pool.brain[a.i]];
}

const BehaviorState& stateOf(const ActorRef& a,
                             const ActorTickContext& context) {
  return brainOf(a, context).behavior.states[a.pool.state[a.i]];
}

uint8_t clearanceOf(const ActorRef& a, const spatial::NavGrid& grid) {
  return grid.requiredClearance(a.pool.radius[a.i]);
}

std::optional<spatial::GridCell> openCellNear(const spatial::NavGrid& grid,
                                              Vec2 point, uint8_t clearance) {
  const std::optional<spatial::GridCell> cell = grid.cellAt(point);
  if (!cell) {
    return std::nullopt;
  }
  return spatial::nearestOpenCell(grid, *cell, clearance, ACTOR_SNAP_RINGS);
}

std::optional<Vec2> openPointNear(const spatial::NavGrid& grid, Vec2 point,
                                  uint8_t clearance) {
  if (grid.cellCount() == 0) {
    return point;
  }
  const std::optional<spatial::GridCell> cell =
      openCellNear(grid, point, clearance);
  if (!cell) {
    return std::nullopt;
  }
  return grid.centre(*cell);
}

}  // namespace eng::game
