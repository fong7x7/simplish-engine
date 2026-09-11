#pragma once

/// @file actor-passes.h
/// @brief The passes `stepActors` makes over the pool, one actor at a time.
/// @par Threading
/// Main-thread-only; called from `stepActors`.

#include "actor-ref.h"

#include <engine/spatial/flow-field.h>
#include <game/actors/actor-intent.h>
#include <game/actors/actor-tick-context.h>
#include <game/actors/actor-workspace.h>
#include <game/player/player-pool.h>
#include <span>
#include <vector>

namespace eng::game {

/// Every actor perceives, decides, intends and plans, one pass each.
void thinkActors(ActorPool& pool, const ActorTickContext& context,
                 ActorWorkspace& workspace);

/// Every actor steers, is separated, moves and turns.
void moveActors(ActorPool& pool, const ActorTickContext& context,
                ActorWorkspace& workspace);

/// Spend this tick's budget rebuilding the players' flow fields.
void advanceFlowFields(const ActorTickContext& context);

/// Work out who actor @p a sees and hears, take or keep a target, and
/// remember or forget where they were — ranking whom it might target in
/// @p workspace, whose neighbour grid finds the actors near it.
void perceiveActor(const ActorRef& a, const ActorTickContext& context,
                   ActorWorkspace& workspace);

/// Take the first interrupt or exit of actor @p a's state whose condition
/// holds, if any.
void decideActor(const ActorRef& a, const ActorTickContext& context,
                 const ActorWorkspace& workspace);

/// Work out where actor @p a's action is taking it into @p intent.
void intendActor(const ActorRef& a, const ActorTickContext& context,
                 ActorIntent& intent);

/// Plan a path for actor @p a when it needs one it does not have.
void planActor(const ActorRef& a, const ActorTickContext& context,
               ActorWorkspace& workspace);

/// Plan actor @p a's way to its goal: down @p field — the flow field of the
/// player it is chasing, or null — when it has one and is far off, else
/// straight when it can walk straight, else by A*.
void planWith(const ActorRef& a, const ActorTickContext& context,
              ActorWorkspace& workspace, const spatial::FlowField* field);

/// Carry out actor @p a's state's attack, if it attacks and can: into the
/// tick's effects buffer, never onto anyone directly.
void attackActor(const ActorRef& a, const ActorTickContext& context);

/// Work out the step actor @p a takes toward its goal into @p intent.
void steerActor(const ActorRef& a, const ActorTickContext& context,
                ActorIntent& intent);

/// Bucket every actor by where it stands, for the passes that ask who is
/// near whom.
void gatherNeighbors(const ActorPool& pool, ActorWorkspace& workspace);

/// Add to each actor's step what it takes to ease it out of the actors
/// near it and out of players' way.
void separateActors(const ActorPool& pool, const PlayerPool& players,
                    ActorWorkspace& workspace);

/// Move actor @p a by its step, out of the level's props — gathering the
/// ones near it into @p boxes — and note whether it was stopped.
void moveActor(const ActorRef& a, const ActorTickContext& context,
               ActorIntent& intent, std::vector<uint32_t>& boxes);

/// Turn actor @p a toward what its state faces.
void faceActor(const ActorRef& a, const ActorTickContext& context,
               const ActorIntent& intent);

}  // namespace eng::game
