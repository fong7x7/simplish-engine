#pragma once

/// @file actor-arena.h
/// @brief A small world of players, props and actors for testing actor
/// behavior headless, stepped one tick at a time.
/// @par Threading
/// Test code; one thread.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/math/vec2.h>
#include <engine/physics/box-broadphase.h>
#include <engine/physics/collision-box.h>
#include <engine/sim/tick-input.h>
#include <engine/spatial/nav-grid.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-flow-fields.h>
#include <game/actors/actor-pool.h>
#include <game/actors/actor-route.h>
#include <game/actors/actor-spawn.h>
#include <game/actors/actor-workspace.h>
#include <game/content/behavior-definition.h>
#include <game/player/player-pool.h>
#include <string>
#include <vector>

namespace eng::game::test {

/// Players standing where a test puts them, props, and actors, on a
/// navigation grid covering 40 × 40 tiles centred near the origin. Players
/// never move unless a test moves them: the arena steps the actors only.
class ActorArena {
public:
  /// An arena with @p obstacles for props.
  explicit ActorArena(std::vector<physics::CollisionBox> obstacles = {});

  /// Add a player standing at @p at; their dense index.
  uint32_t addPlayer(Vec2 at);
  /// Add an actor running @p behavior, spawned as @p spawn says — whose
  /// `behavior` is ignored; its dense index.
  uint32_t addActor(const BehaviorDefinition& behavior, ActorSpawn spawn);
  /// Put player @p player at @p at.
  void movePlayer(uint32_t player, Vec2 at);
  /// Have player @p player hold fire, or let go.
  void setFiring(uint32_t player, uint32_t buttons);
  /// Step the actors @p ticks ticks.
  void step(uint32_t ticks = 1);

  /// Where actor @p actor stands, on the floor.
  [[nodiscard]] Vec2 actorAt(uint32_t actor) const;
  /// The id of the state actor @p actor is in.
  [[nodiscard]] const std::string& stateOf(uint32_t actor) const;
  /// A hash of every actor's state.
  [[nodiscard]] uint64_t hash() const;

  /// The players.
  PlayerPool players;
  /// The props.
  std::vector<physics::CollisionBox> obstacles;
  /// Where actors can go.
  spatial::NavGrid grid;
  /// The props, bucketed.
  physics::BoxBroadphase broadphase;
  /// The players' flow fields.
  ActorFlowFields flow;
  /// The brains added actors run.
  std::vector<ActorBrain> brains;
  /// The routes added actors patrol.
  std::vector<ActorRoute> routes;
  /// The actors.
  ActorPool actors{16};
  /// Scratch for the actor passes.
  ActorWorkspace workspace;
  /// The AI stream.
  Pcg32 rng{7, 1};
  /// The next tick to step.
  uint64_t tick = 0;
  /// Every player's input for the next tick.
  sim::TickInput input{};
};

}  // namespace eng::game::test
