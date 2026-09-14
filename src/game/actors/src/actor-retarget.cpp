// Keeping a path up with a goal that moves — a quarry out of sight — by
// changing its end rather than planning it again.

#include "actor-passes.h"
#include "actor-queries.h"
#include "actor-tuning.h"

#include <algorithm>
#include <engine/spatial/line-of-sight.h>
#include <engine/spatial/path-smoothing.h>
#include <span>

namespace eng::game {

namespace {

  /// The earliest of actor @p a's waypoints still ahead from which it, and
  /// every one after it, has a straight walk to @p to — scanning back from
  /// the last and stopping at the first that has none. `count` when not
  /// even the last has one.
  uint32_t firstSeeing(const ActorRef& a, const ActorTickContext& context,
                       Vec2 to) {
    const ActorPath& path = a.pool.path[a.i];
    const uint8_t clearance = clearanceOf(a, context.grid);
    uint32_t first = path.count;
    while (first > path.next &&
           spatial::hasLineOfSight(context.grid, path.points[first - 1], to,
                                   clearance)) {
      --first;
    }
    return first;
  }

  /// Point actor @p a's path at @p goal from the earliest of its last
  /// waypoints with a straight walk there, dropping the ones after it.
  /// False when not even its last has one, or there is no room.
  bool reaim(const ActorRef& a, const ActorTickContext& context,
             spatial::GridCell goal) {
    ActorPath& path = a.pool.path[a.i];
    const Vec2 to = context.grid.centre(goal);
    const uint32_t first = firstSeeing(a, context, to);
    if (first == path.count || first + 1 >= ACTOR_PATH_POINTS) {
      return false;
    }
    path.points[first + 1] = to;
    path.count = first + 2;
    path.goal = goal;
    return true;
  }

  /// Whether actor @p a's path may be mended toward @p goal rather than
  /// planned again: it was planned within `ACTOR_REPAIR_TICKS`, has room
  /// for more waypoints, and ends within `ACTOR_REPAIR_TILES` of the goal.
  bool mendable(const ActorRef& a, const ActorTickContext& context,
                spatial::GridCell goal) {
    const ActorPath& path = a.pool.path[a.i];
    constexpr float NEAR = ACTOR_REPAIR_TILES * ACTOR_REPAIR_TILES;
    return context.tick - path.planned_tick < ACTOR_REPAIR_TICKS &&
           path.count < ACTOR_PATH_POINTS &&
           Vec2::distanceSquared(path.points[path.count - 1],
                                 context.grid.centre(goal)) <= NEAR;
  }

  /// Search from the end of actor @p a's path to @p goal, within
  /// `ACTOR_REPAIR_EXPANSIONS` and what is left of the tick's budget.
  spatial::PathResult searchOn(const ActorRef& a,
                               const ActorTickContext& context,
                               ActorWorkspace& workspace,
                               spatial::GridCell goal) {
    const ActorPath& path = a.pool.path[a.i];
    const auto from = context.grid.cellAt(path.points[path.count - 1]);
    if (!from) {
      return {};
    }
    const uint32_t allowed =
        std::min(workspace.path_budget, ACTOR_REPAIR_EXPANSIONS);
    const spatial::PathResult result = workspace.finder.find(
        context.grid, {*from, goal, clearanceOf(a, context.grid), allowed});
    workspace.path_budget -= std::min(result.expanded, workspace.path_budget);
    return result;
  }

  /// Mend actor @p a's path toward @p goal: search on from its end and
  /// splice what that finds, smoothed, on after it. False when it may not
  /// be mended, or the search finds no way.
  bool repair(const ActorRef& a, const ActorTickContext& context,
              ActorWorkspace& workspace, spatial::GridCell goal) {
    if (!mendable(a, context, goal) || workspace.path_budget == 0) {
      return false;
    }
    const spatial::PathResult found = searchOn(a, context, workspace, goal);
    if (found.status != spatial::PathStatus::FOUND) {
      return false;
    }
    ActorPath& path = a.pool.path[a.i];
    const std::span<Vec2> room = std::span(path.points).subspan(path.count);
    path.count += static_cast<uint32_t>(spatial::smoothPath(
        context.grid, found.cells, clearanceOf(a, context.grid), room));
    path.goal = goal;
    return true;
  }

}  // namespace

bool retargetPath(const ActorRef& a, const ActorTickContext& context,
                  ActorWorkspace& workspace, spatial::GridCell goal) {
  const ActorPath& path = a.pool.path[a.i];
  if (path.next >= path.count) {
    return false;
  }
  return path.goal == goal || reaim(a, context, goal) ||
         repair(a, context, workspace, goal);
}

}  // namespace eng::game
