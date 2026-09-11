#pragma once

/// @file actor-conditions.h
/// @brief Testing one behavior exit's condition against one actor.
/// @par Threading
/// Main-thread-only; may draw from the tick's AI stream.

#include "actor-ref.h"

#include <game/actors/actor-tick-context.h>
#include <game/actors/actor-workspace.h>
#include <game/content/behavior-exit.h>

namespace eng::game {

/// Whether @p exit's condition holds of actor @p a this tick — asking
/// @p workspace's neighbour grid who is near, for `allies_within`. A
/// `chance` condition draws once from the AI stream each time it is
/// tested.
[[nodiscard]] bool conditionHolds(const ActorRef& a,
                                  const ActorTickContext& context,
                                  const ActorWorkspace& workspace,
                                  const BehaviorExit& exit);

}  // namespace eng::game
