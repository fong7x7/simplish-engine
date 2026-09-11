#pragma once

/// @file actor-tick-context.h
/// @brief Everything the actor passes read from outside the actor pool.
/// @par Threading
/// A view over the world's state for one tick.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/physics/box-broadphase.h>
#include <engine/physics/collision-box.h>
#include <engine/sim/tick-input.h>
#include <engine/spatial/nav-grid.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-flow-fields.h>
#include <game/actors/actor-route.h>
#include <game/player/player-pool.h>
#include <span>

namespace eng::game {

/// One tick's view of the world, as the actors see it: the clock, the
/// players and their input, the level, the brains, the flow fields, and
/// the AI stream. Built by the world for each tick and discarded with it.
struct ActorTickContext {
  /// The tick being simulated.
  uint64_t tick = 0;
  /// Every player's input this tick: what hearing listens to.
  const sim::TickInput& input;
  /// The players, moved already this tick: what actors perceive.
  const PlayerPool& players;
  /// Where actors can go.
  const spatial::NavGrid& grid;
  /// The level's solid geometry, which movement stops against.
  std::span<const physics::CollisionBox> obstacles;
  /// `obstacles`, bucketed, so movement tests only the boxes near it.
  const physics::BoxBroadphase& broadphase;
  /// The brains actors run, indexed by `ActorPool::brain`.
  std::span<const ActorBrain> brains;
  /// The routes actors patrol, indexed by `ActorPool::route`.
  std::span<const ActorRoute> routes;
  /// The players' flow fields, which the tick advances and pursuers walk.
  ActorFlowFields& flow;
  /// The simulation's AI stream: wander spots and `chance` draws. Drawn
  /// from in dense order, so every peer draws the same numbers for the
  /// same actors.
  Pcg32& rng;
};

}  // namespace eng::game
