#pragma once

/// @file deployed-game-run.h
/// @brief How a run of a deployed game went.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <engine/sim/tick-hash.h>
#include <game/logic/run-outcome.h>
#include <string>
#include <vector>

namespace eng::editor {

/// The outcome of `runDeployedGame`.
/// @thread_safety Immutable value type.
struct DeployedGameRun {
  /// Why the game could not run; empty when it ran.
  std::string error{};
  /// The level played.
  std::string level{};
  /// Players seated, every one a stand-in.
  uint8_t players = 0;
  /// Ticks simulated.
  uint64_t ticks = 0;
  /// How the run stood when it stopped.
  game::RunOutcome outcome = game::RunOutcome::PLAYING;
  /// The last tick's state hash: two runs of one build, level, player
  /// count and length agree on it, on every platform (ADR-002).
  uint64_t hash = 0;
  /// Whether the project's game logic ran.
  bool logic = false;
  /// Every tick's hash, in order, when the options asked for them.
  std::vector<sim::TickHash> tick_hashes{};
};

}  // namespace eng::editor
