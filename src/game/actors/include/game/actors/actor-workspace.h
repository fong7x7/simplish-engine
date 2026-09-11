#pragma once

/// @file actor-workspace.h
/// @brief Scratch memory the actor passes share, sized once per run.
/// @par Threading
/// Main-thread-only; used only inside a tick.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/physics/box-broadphase.h>
#include <engine/spatial/nav-grid.h>
#include <engine/spatial/neighbor-grid.h>
#include <engine/spatial/path-finder.h>
#include <game/actors/actor-candidate.h>
#include <game/actors/actor-intent.h>
#include <vector>

namespace eng::game {

/// Most cells all the path searches of one tick may expand between them,
/// so one tick of actors replanning at once has a ceiling on what it costs.
/// A request the budget cannot cover waits for the next tick.
inline constexpr uint32_t ACTOR_PATH_BUDGET_PER_TICK = 32768;

/// What the actor passes work in and throw away: the path finder's
/// scratch, the neighbour grid, and each actor's intent for the tick. None
/// of it is state — each tick rebuilds it from state before reading it — so
/// none of it is hashed; it is sized when the world is built, so a tick
/// allocates nothing.
struct ActorWorkspace {
  /// A workspace for @p actor_capacity actors on @p grid, colliding with
  /// the boxes in @p broadphase.
  ActorWorkspace(uint32_t actor_capacity, const spatial::NavGrid& grid,
                 const physics::BoxBroadphase& broadphase);

  /// The planner every actor's path requests go through, in turn.
  spatial::PathFinder finder;
  /// Each actor's intent for the tick, by dense index.
  std::vector<ActorIntent> intents;
  /// Where each actor stood at the start of the tick, on the floor, by
  /// dense index: what `neighbors` was built from.
  std::vector<Vec2> positions;
  /// Every actor, bucketed by where it stood at the start of the tick.
  spatial::NeighborGrid neighbors;
  /// The largest radius among the actors, this tick: how much further than
  /// its own radius an actor looks for others it could overlap.
  float largest_radius = 0.0F;
  /// The boxes near the actor being moved, as the broadphase gathers them.
  std::vector<uint32_t> boxes;
  /// Whom the actor perceiving might take as its target, ranked.
  std::vector<ActorCandidate> candidates;
  /// Expansions left in this tick's path budget.
  uint32_t path_budget = 0;
};

}  // namespace eng::game
