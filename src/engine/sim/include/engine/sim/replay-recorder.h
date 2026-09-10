#pragma once

/// @file replay-recorder.h
/// @brief Builds a replay from a run as it is simulated.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/sim/replay-header.h>
#include <engine/sim/replay.h>
#include <engine/sim/tick-hash.h>
#include <engine/sim/tick-input.h>
#include <engine/sim/tick-result.h>
#include <optional>

namespace eng::sim {

/// Ticks between checkpoints by default: one a second, which keeps ten
/// minutes of checkpoints to a few kilobytes while still placing a
/// divergence within a second of where it happened.
inline constexpr uint64_t DEFAULT_CHECKPOINT_INTERVAL = 60;

/// Records every tick's input and a checkpoint every `checkpoint_interval`
/// ticks. Replays are recorded always, in every build (Engine §4.4).
///
/// Call `record` after each `Simulation::step`, outside the tick. Space for
/// ten minutes of input is reserved up front; a longer run grows the buffer
/// between ticks, never inside one.
class ReplayRecorder {
public:
  /// A recorder for a run starting from `header`, checkpointing every
  /// `checkpoint_interval` ticks (at least 1).
  ReplayRecorder(ReplayHeader header, uint64_t checkpoint_interval);

  /// Records the tick `result` describes, which ran on `input`. Ticks must
  /// arrive in order from 0. A result without a hash records the input and
  /// no checkpoint — the replay still plays, but cannot be verified.
  void record(const TickInput& input, const TickResult& result);

  /// The replay so far, with the last recorded hash as its final checkpoint.
  [[nodiscard]] Replay finish() const;

private:
  /// Inputs and interval checkpoints recorded so far.
  Replay replay_;
  /// Ticks between checkpoints.
  uint64_t checkpoint_interval_;
  /// The most recent tick hash, which `finish` appends if it is not already
  /// an interval checkpoint.
  std::optional<TickHash> last_hash_;
};

}  // namespace eng::sim
