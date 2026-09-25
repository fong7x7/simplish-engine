#pragma once

/// @file world-logic-scene.h
/// @brief What of the world a project's game logic is shown for one tick.
/// @par Threading
/// A view over the world's state for one tick.

#include <engine/core/pcg32.h>
#include <engine/sim/tick-context.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-pool.h>
#include <game/actors/actor-spawn.h>
#include <game/content/game-content.h>
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
  /// Where the logic's damage and healing are queued.
  std::vector<LogicCommand>& commands;
  /// Where the logic's spawns are queued.
  std::vector<ActorSpawn>& spawns;
  /// The run's content: the enemy archetypes `spawnEnemy` names.
  const GameContent& content;
  /// The logic's own random stream.
  Pcg32& rng;
  /// How the run stands; the logic may end it.
  RunOutcome& outcome;
  /// Where what the logic says is kept for whoever runs the game.
  std::vector<std::string>& log;
};

}  // namespace eng::game
