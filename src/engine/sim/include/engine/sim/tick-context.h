#pragma once

/// @file tick-context.h
/// @brief What every phase of a tick is given.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/tick-input.h>

namespace eng::sim {

/// Everything a phase may know about the tick it runs in. There is no
/// elapsed time: a tick is always 1/60 s, so the tick number is the clock.
struct TickContext {
  /// The tick being simulated, counted from 0 at the start of the run.
  uint64_t tick = 0;
  /// Every player's input for this tick.
  TickInput input{};
};

}  // namespace eng::sim
