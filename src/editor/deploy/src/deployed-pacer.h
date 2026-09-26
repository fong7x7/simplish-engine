#pragma once

/// @file deployed-pacer.h
/// @brief How many inputs a networked deployed game owes the session now.
/// @par Threading Main-thread-only.

#include <chrono>
#include <cstdint>
#include <editor/deploy/deployed-pace.h>
#include <engine/core/fixed-step-clock.h>

namespace eng::editor {

/// Paces a networked run's input on real time, or not at all. Reads the
/// wall clock, which is allowed here and nowhere in a tick: it decides only
/// *when* input is sampled; frames decide what is simulated (ADR-013).
/// @thread_safety Main-thread-only.
class DeployedPacer {
public:
  /// A pacer at @p pace, starting now.
  explicit DeployedPacer(DeployedPace pace);

  /// Inputs owed since the last call: the ticks real time has paid for,
  /// or one when not pacing.
  [[nodiscard]] uint32_t due();

  /// Give the rest of this moment back to the system, when pacing — a
  /// server loop should not spin a core waiting for packets.
  void rest() const;

private:
  /// Real time, or none.
  DeployedPace pace_;
  /// Real time into whole ticks.
  FixedStepClock clock_;
  /// When `due` was last asked.
  std::chrono::steady_clock::time_point last_;
};

}  // namespace eng::editor
