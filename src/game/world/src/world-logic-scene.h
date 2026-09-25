#pragma once

/// @file world-logic-scene.h
/// @brief What of the world a project's game logic is shown for one tick.
/// @par Threading
/// A view over the world's state for one tick.

#include "world-logic-output.h"
#include "world-logic-run.h"
#include "world-logic-writes.h"

#include <engine/core/pcg32.h>
#include <engine/physics/collision-box.h>
#include <engine/sim/tick-context.h>
#include <engine/spatial/nav-grid.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-pool.h>
#include <game/actors/actor-spawn.h>
#include <game/combat/combat-effects.h>
#include <game/content/game-content.h>
#include <game/logic/logic-event.h>
#include <game/logic/run-outcome.h>
#include <game/player/player-pool.h>
#include <game/world/logic-command.h>
#include <span>
#include <string>
#include <vector>

namespace eng::game {

/// The world's state a `WorldLogicView` reads and the queues it writes to,
/// borrowed from the `GameWorld` for one run of its logic.
struct WorldLogicScene {
  /// The tick being simulated, and its input.
  const sim::TickContext& context;
  /// Every player.
  const PlayerPool& players;
  /// Every actor.
  const ActorPool& actors;
  /// The brains actors run, which name the state each is in.
  std::span<const ActorBrain> brains;
  /// The level's name for each actor, by its handle's slot.
  std::span<const std::string> actor_ids;
  /// Where the logic's writes are queued: commands, spawns, combat.
  WorldLogicWrites writes;
  /// The run's content: the enemy archetypes `spawnEnemy` names.
  const GameContent& content;
  /// Where actors can go: what line of sight and walkability are asked of.
  const spatial::NavGrid& grid;
  /// The level's solid geometry.
  std::span<const physics::CollisionBox> obstacles;
  /// What happened last tick.
  std::span<const LogicEvent> events;
  /// The logic's own random stream.
  Pcg32& rng;
  /// How the logic has ended the run, and whose steps it hears.
  WorldLogicRun run;
  /// Where what the logic says and cues is kept for presentation.
  WorldLogicOutput output;
};

}  // namespace eng::game
