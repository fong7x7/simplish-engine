#pragma once

/// @file world-read-scratch.h
/// @brief Where a read view's writes go: nowhere that matters.
/// @par Threading
/// Main-thread-only.

#include <engine/core/pcg32.h>
#include <engine/sim/tick-context.h>
#include <game/actors/actor-spawn.h>
#include <game/combat/combat-effects.h>
#include <game/logic/run-outcome.h>
#include <game/world/logic-command.h>
#include <string>
#include <vector>

namespace eng::game {

/// The queues and copies a `WorldReadView` points its writes at, so the
/// world between ticks can be read through `GameLogicWorld` without being
/// changed. A base of the view, so it exists before the view is built over
/// it.
struct WorldReadScratch {
  /// The tick being read: the last one simulated.
  sim::TickContext context{};
  /// Damage and the rest, never applied.
  std::vector<LogicCommand> commands{};
  /// Spawns, never applied.
  std::vector<ActorSpawn> spawns{};
  /// Shots, blasts and pools, never applied.
  CombatEffects combat{};
  /// A copy of the logic's stream.
  Pcg32 rng;
  /// A copy of the outcome.
  RunOutcome outcome = RunOutcome::PLAYING;
  /// Lines logged, never shown.
  std::vector<std::string> log{};
};

}  // namespace eng::game
