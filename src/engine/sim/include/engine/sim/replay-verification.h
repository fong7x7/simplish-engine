#pragma once

/// @file replay-verification.h
/// @brief Plays a replay back and checks it reproduces its recording.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/sim/hash-divergence.h>
#include <engine/sim/replay.h>
#include <engine/sim/simulation.h>
#include <optional>

namespace eng::sim {

/// Whether a playback reproduced the run it was recorded from.
struct ReplayVerification {
  /// Ticks simulated before verification finished or stopped.
  uint64_t ticks_run = 0;
  /// The first checkpoint the playback disagreed with; nothing when every
  /// checkpoint matched.
  std::optional<HashDivergence> divergence;

  /// True when every checkpoint matched.
  [[nodiscard]] bool ok() const { return !divergence.has_value(); }
};

/// Steps `simulation` through every input in `replay`, comparing each
/// checkpoint as its tick is reached, and stops at the first that differs.
///
/// The caller builds `simulation` from the replay's header — its level,
/// content, seed and player count — with hashing on and at tick 0. That is
/// the whole replay-corpus CI job (Development REQUIREMENTS §5.1), and the
/// fastest way to learn whether a change altered what the game simulates.
ReplayVerification verifyReplay(const Replay& replay, Simulation& simulation);

}  // namespace eng::sim
