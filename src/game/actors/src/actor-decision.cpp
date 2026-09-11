#include "actor-conditions.h"
#include "actor-passes.h"
#include "actor-queries.h"

#include <optional>
#include <span>

namespace eng::game {

namespace {

  /// The state the first of @p exits that holds leads to, skipping any that
  /// lead to the state actor @p a is already in.
  std::optional<uint8_t> firstExit(const ActorRef& a,
                                   const ActorTickContext& context,
                                   const ActorWorkspace& workspace,
                                   std::span<const BehaviorExit> exits) {
    for (const BehaviorExit& exit : exits) {
      if (exit.to != a.pool.state[a.i] &&
          conditionHolds(a, context, workspace, exit)) {
        return exit.to;
      }
    }
    return std::nullopt;
  }

  /// Put actor @p a in state @p next, starting it afresh: no goal, no path,
  /// and nothing from how the last state's movement went.
  void enterState(const ActorRef& a, uint8_t next, uint64_t tick) {
    a.pool.state[a.i] = next;
    a.pool.state_since[a.i] = tick;
    a.pool.has_goal[a.i] = 0;
    a.pool.arrived[a.i] = 0;
    a.pool.blocked[a.i] = 0;
    a.pool.no_path[a.i] = 0;
    a.pool.path[a.i] = {};
  }

}  // namespace

void decideActor(const ActorRef& a, const ActorTickContext& context,
                 const ActorWorkspace& workspace) {
  const BehaviorDefinition& behavior = brainOf(a, context).behavior;
  std::optional<uint8_t> next =
      firstExit(a, context, workspace, behavior.interrupts);
  if (!next) {
    next = firstExit(a, context, workspace, stateOf(a, context).exits);
  }
  if (next) {
    enterState(a, *next, context.tick);
  }
}

}  // namespace eng::game
