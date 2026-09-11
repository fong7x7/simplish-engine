#pragma once

/// @file actor-workspace.h
/// @brief Scratch memory the actor passes share, sized once per run.
/// @par Threading
/// Main-thread-only; used only inside a tick.

#include <cstdint>
#include <engine/spatial/path-finder.h>
#include <game/actors/actor-intent.h>
#include <vector>

namespace eng::game {

/// Most cells all the path searches of one tick may expand between them,
/// so one tick of actors replanning at once has a ceiling on what it costs.
/// A request the budget cannot cover waits for the next tick.
inline constexpr uint32_t ACTOR_PATH_BUDGET_PER_TICK = 32768;

/// What the actor passes work in and throw away: the path finder's
/// scratch, and each actor's intent for the tick. None of it is state, so
/// none of it is hashed; it is sized when the world is built, so a tick
/// allocates nothing.
struct ActorWorkspace {
  /// A workspace for @p actor_capacity actors on a grid of @p grid_cells.
  ActorWorkspace(uint32_t actor_capacity, uint32_t grid_cells);

  /// The planner every actor's path requests go through, in turn.
  spatial::PathFinder finder;
  /// Each actor's intent for the tick, by dense index.
  std::vector<ActorIntent> intents;
  /// Expansions left in this tick's path budget.
  uint32_t path_budget = 0;
};

}  // namespace eng::game
