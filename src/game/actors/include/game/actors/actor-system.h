#pragma once

/// @file actor-system.h
/// @brief What the simulation does to actors each tick.
/// @par Threading
/// Main-thread-only; called from the tick's phases.

#include <cstdint>
#include <engine/sim/entity-handle.h>
#include <engine/sim/state-hasher.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-pool.h>
#include <game/actors/actor-spawn.h>
#include <game/actors/actor-tick-context.h>
#include <game/actors/actor-workspace.h>
#include <optional>

namespace eng::game {

/// Add an actor as @p spawn describes, running brain @p brain_index — which
/// is @p brain — in that brain's initial state. Nothing when the pool is
/// full.
std::optional<sim::EntityHandle> spawnActor(ActorPool& pool,
                                            const ActorSpawn& spawn,
                                            uint16_t brain_index,
                                            const ActorBrain& brain);

/// §4.1 step 3: every actor perceives, decides, plans, steers, moves and
/// turns, in that order, each a linear pass over the pool in dense order.
///
/// - **Perceive.** Each player in range, in the view cone and in line of
///   sight is seen; one who fired within hearing range is heard. The actor
///   keeps its target while it still perceives them, and otherwise takes
///   the nearest it does. It remembers where it last perceived them, and
///   forgets them after its behavior's memory.
/// - **Decide.** Interrupts, then the current state's exits, in the order
///   written; the first whose condition holds is taken.
/// - **Intend.** The state's action picks where to go and how close counts.
/// - **Plan.** An actor with a straight walk to its goal takes it. One
///   without asks the planner, within this tick's path budget, and follows
///   the smoothed waypoints it gets back.
/// - **Steer and move.** Toward the next waypoint at the state's speed,
///   eased apart from other actors and out of players' way, then pushed out
///   of the level's props exactly as a player is.
/// - **Face.** Turned toward what the state faces, at the behavior's rate.
///
/// Actors never move players, and players are never blocked by actors.
void stepActors(ActorPool& pool, const ActorTickContext& context,
                ActorWorkspace& workspace);

/// §4.1 step 8: destroy the actors marked for it.
void compactActors(ActorPool& pool);

/// Fold every actor's state into a tick hash section.
void hashActors(const ActorPool& pool, sim::StateHasher& hasher);

}  // namespace eng::game
