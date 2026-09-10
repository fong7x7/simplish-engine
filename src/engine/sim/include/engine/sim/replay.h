#pragma once

/// @file replay.h
/// @brief A recorded run: initial conditions, every input, and checkpoints.
/// @par Threading
/// A value type.

#include <engine/sim/replay-header.h>
#include <engine/sim/tick-hash.h>
#include <engine/sim/tick-input.h>
#include <vector>

namespace eng::sim {

/// A run as a replay: what it started from, what every player did on every
/// tick, and state hashes to check a playback against.
///
/// Because the simulation is deterministic, this is enough to reproduce the
/// run exactly — which makes it a bug report, a regression test, and, in a
/// lockstep session, the same data the peers exchanged (ADR-005).
struct Replay {
  /// The run's initial conditions.
  ReplayHeader header;
  /// Every tick's input; the input for tick N is at index N.
  std::vector<TickInput> inputs;
  /// Tick hashes at regular intervals and at the final tick, in tick order.
  /// Their section hashes are kept and their names are not.
  std::vector<TickHash> checkpoints;
};

}  // namespace eng::sim
