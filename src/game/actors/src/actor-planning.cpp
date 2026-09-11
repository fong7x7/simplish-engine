#include "actor-passes.h"
#include "actor-queries.h"
#include "actor-tuning.h"

#include <algorithm>
#include <engine/spatial/line-of-sight.h>
#include <engine/spatial/path-smoothing.h>

namespace eng::game {

namespace {

  /// Drop actor @p a's path: it walks straight to its goal.
  void walkStraight(const ActorRef& a) {
    a.pool.path[a.i].count = 0;
    a.pool.path[a.i].next = 0;
    a.pool.no_path[a.i] = 0;
  }

  /// Record that no path takes actor @p a toward @p goal as of @p tick.
  void markNoPath(const ActorRef& a, spatial::GridCell goal, uint64_t tick) {
    a.pool.path[a.i] = {.goal = goal, .planned_tick = tick};
    a.pool.no_path[a.i] = 1;
  }

  /// Whether actor @p a needs a new plan to reach @p goal: its path leads
  /// elsewhere or has run out, and it has not planned too recently.
  bool needsPlan(const ActorRef& a, uint64_t tick, spatial::GridCell goal) {
    const ActorPath& path = a.pool.path[a.i];
    const bool following = path.next < path.count;
    if (following && path.goal == goal) {
      return false;
    }
    const bool recent = tick - path.planned_tick < ACTOR_REPLAN_TICKS;
    return !recent || (!following && a.pool.no_path[a.i] == 0);
  }

  /// Store @p result as actor @p a's path to @p goal.
  void storePath(const ActorRef& a, const ActorTickContext& context,
                 const spatial::PathResult& result, spatial::GridCell goal) {
    if (result.status != spatial::PathStatus::FOUND) {
      markNoPath(a, goal, context.tick);
      return;
    }
    ActorPath& path = a.pool.path[a.i];
    path = {.goal = goal, .planned_tick = context.tick};
    path.count = static_cast<uint32_t>(spatial::smoothPath(
        context.grid, result.cells, clearanceOf(a, context.grid), path.points));
    a.pool.no_path[a.i] = 0;
  }

  /// Plan actor @p a's path to @p goal, within what is left of the tick's
  /// budget. A search the budget cut short is tried again next tick.
  void plan(const ActorRef& a, const ActorTickContext& context,
            ActorWorkspace& workspace, spatial::GridCell goal) {
    const uint8_t clearance = clearanceOf(a, context.grid);
    const auto from =
        openCellNear(context.grid, flat(a.pool.position[a.i]), clearance);
    if (!from) {
      markNoPath(a, goal, context.tick);
      return;
    }
    const uint32_t allowed =
        std::min(workspace.path_budget, spatial::PATH_DEFAULT_MAX_EXPANSIONS);
    const spatial::PathResult result =
        workspace.finder.find(context.grid, {*from, goal, clearance, allowed});
    workspace.path_budget -= std::min(result.expanded, workspace.path_budget);
    if (result.status != spatial::PathStatus::OVER_BUDGET ||
        allowed == spatial::PATH_DEFAULT_MAX_EXPANSIONS) {
      storePath(a, context, result, goal);
    }
  }

  /// Plan actor @p a a path toward @p goal, which it cannot walk straight
  /// to, when it needs one and the tick's budget has any left.
  void planToward(const ActorRef& a, const ActorTickContext& context,
                  ActorWorkspace& workspace, Vec2 goal) {
    const auto goal_cell =
        openCellNear(context.grid, goal, clearanceOf(a, context.grid));
    if (!goal_cell) {
      markNoPath(a, {}, context.tick);
    } else if (needsPlan(a, context.tick, *goal_cell) &&
               workspace.path_budget > 0) {
      plan(a, context, workspace, *goal_cell);
    }
  }

  /// The complete flow field toward the player actor @p a is chasing, when
  /// it perceives them this tick and is no wider than the field allows;
  /// null otherwise.
  const spatial::FlowField* fieldFor(const ActorRef& a,
                                     const ActorTickContext& context,
                                     const ActorIntent& intent) {
    const auto player = context.players.slots.denseIndex(a.pool.target[a.i]);
    if (intent.chases == 0 || !player ||
        a.pool.target_kind[a.i] != CombatantKind::PLAYER) {
      return nullptr;
    }
    const uint8_t slot = context.players.input_slot[*player];
    const spatial::FlowField& field =
        context.flow.fields[slot % context.flow.fields.size()];
    const bool fits = clearanceOf(a, context.grid) <= field.clearance();
    return field.complete() && fits ? &field : nullptr;
  }

  /// Aim actor @p a a few cells down @p field: a path of one waypoint,
  /// worked out again every tick, which costs a handful of lookups where a
  /// search would cost thousands.
  void walkField(const ActorRef& a, const ActorTickContext& context,
                 const spatial::FlowField& field) {
    const auto from = openCellNear(context.grid, flat(a.pool.position[a.i]),
                                   field.clearance());
    const auto ahead =
        from ? field.descend(context.grid, *from, ACTOR_FLOW_LOOKAHEAD_CELLS)
             : std::nullopt;
    if (!ahead) {
      markNoPath(a, field.goal(), context.tick);
      return;
    }
    ActorPath& path = a.pool.path[a.i];
    path = {.goal = field.goal(), .planned_tick = context.tick};
    path.points[0] = context.grid.centre(*ahead);
    path.count = 1;
    a.pool.no_path[a.i] = 0;
  }

  /// Whether actor @p a is going anywhere a path could take it: a charge
  /// runs straight whatever is ahead.
  bool travels(const ActorRef& a, const ActorTickContext& context,
               const ActorWorkspace& workspace) {
    return workspace.intents[a.i].moves != 0 &&
           stateOf(a, context).action != BehaviorAction::CHARGE;
  }

}  // namespace

void planActor(const ActorRef& a, const ActorTickContext& context,
               ActorWorkspace& workspace) {
  if (travels(a, context, workspace)) {
    planWith(a, context, workspace,
             fieldFor(a, context, workspace.intents[a.i]));
  }
}

void planWith(const ActorRef& a, const ActorTickContext& context,
              ActorWorkspace& workspace, const spatial::FlowField* field) {
  const Vec2 at = flat(a.pool.position[a.i]);
  const Vec2 goal = a.pool.goal[a.i];
  constexpr float DIRECT = ACTOR_FLOW_DIRECT_TILES * ACTOR_FLOW_DIRECT_TILES;
  // Far from its quarry a pursuer keeps to the field without asking what
  // it could see; near, a straight walk is closer to what it would do.
  const bool far_off =
      field != nullptr && Vec2::distanceSquared(at, goal) > DIRECT;
  if (!far_off && spatial::hasLineOfSight(context.grid, at, goal,
                                          clearanceOf(a, context.grid))) {
    walkStraight(a);
  } else if (field != nullptr) {
    walkField(a, context, *field);
  } else {
    planToward(a, context, workspace, goal);
  }
}

}  // namespace eng::game
