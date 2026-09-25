#pragma once

/// @file world-read-sources.h
/// @brief What of a world a read view between ticks is built over.
/// @par Threading
/// A view over the world's state, for as long as the read view lives.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/physics/collision-box.h>
#include <engine/spatial/nav-grid.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-pool.h>
#include <game/content/game-content.h>
#include <game/logic/logic-event.h>
#include <game/logic/run-outcome.h>
#include <game/player/player-pool.h>
#include <game/world/world-ui.h>
#include <span>
#include <string>

namespace eng::game {

/// The world's own state a `WorldReadView` reads, and copies of what it
/// would otherwise change: its random stream and the outcome.
struct WorldReadSources {
  /// Every player.
  const PlayerPool& players;
  /// Every actor.
  const ActorPool& actors;
  /// The brains actors run.
  std::span<const ActorBrain> brains;
  /// Each actor's name, by slot.
  std::span<const std::string> actor_ids;
  /// The run's content.
  const GameContent& content;
  /// The navigation grid.
  const spatial::NavGrid& grid;
  /// The level's solid geometry.
  std::span<const physics::CollisionBox> obstacles;
  /// What happened in the last tick.
  std::span<const LogicEvent> events;
  /// The last tick simulated.
  uint64_t tick = 0;
  /// A copy of the logic's stream, so a read view's dice change nothing.
  Pcg32 rng;
  /// How the run stands.
  RunOutcome outcome = RunOutcome::PLAYING;
  /// A copy of the screens shown.
  WorldUi ui{};
  /// The project's screens, by id.
  std::span<const std::string> screens{};
};

}  // namespace eng::game
