#pragma once

/// @file actor-passes.h
/// @brief The passes `stepActors` makes over the pool, one actor at a time.
/// @par Threading
/// Main-thread-only; called from `stepActors`.

#include "actor-ref.h"

#include <game/actors/actor-intent.h>
#include <game/actors/actor-tick-context.h>
#include <game/actors/actor-workspace.h>
#include <game/player/player-pool.h>
#include <span>

namespace eng::game {

/// Work out who actor @p a sees and hears, take or keep a target, and
/// remember or forget where they were.
void perceiveActor(const ActorRef& a, const ActorTickContext& context);

/// Take the first interrupt or exit of actor @p a's state whose condition
/// holds, if any.
void decideActor(const ActorRef& a, const ActorTickContext& context);

/// Work out where actor @p a's action is taking it into @p intent.
void intendActor(const ActorRef& a, const ActorTickContext& context,
                 ActorIntent& intent);

/// Plan a path for actor @p a when it needs one it does not have.
void planActor(const ActorRef& a, const ActorTickContext& context,
               ActorWorkspace& workspace);

/// Work out the step actor @p a takes toward its goal into @p intent.
void steerActor(const ActorRef& a, const ActorTickContext& context,
                ActorIntent& intent);

/// Add to each actor's step what it takes to ease it out of other actors
/// and out of players' way.
void separateActors(const ActorPool& pool, const PlayerPool& players,
                    std::span<ActorIntent> intents);

/// Move actor @p a by its step, out of the level's props, and note whether
/// it was stopped.
void moveActor(const ActorRef& a, const ActorTickContext& context,
               ActorIntent& intent);

/// Turn actor @p a toward what its state faces.
void faceActor(const ActorRef& a, const ActorTickContext& context,
               const ActorIntent& intent);

}  // namespace eng::game
