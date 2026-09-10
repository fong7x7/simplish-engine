#pragma once

/// @file tick-result.h
/// @brief What one call to `Simulation::step` produced.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/tick-hash.h>
#include <optional>

namespace eng::sim {

/// The outcome of simulating one tick.
struct TickResult {
  /// The tick that was simulated.
  uint64_t tick = 0;
  /// The state hash at the end of it, when the simulation hashes ticks.
  std::optional<TickHash> hash;
};

}  // namespace eng::sim
