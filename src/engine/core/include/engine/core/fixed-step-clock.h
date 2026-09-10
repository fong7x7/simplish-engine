#pragma once

/// @file fixed-step-clock.h
/// @brief The accumulator that turns frame time into 60 Hz ticks.
/// @par Threading
/// Main-thread-only; owned by whatever drives the simulation.

#include <cstdint>
#include <engine/core/fixed-step-advance.h>

namespace eng {

/// Simulation ticks per second (Engine REQUIREMENTS §4.1).
inline constexpr uint32_t TICK_RATE_HZ = 60;

/// Most ticks one frame may run before the rest are dropped, so a slow frame
/// cannot demand a slower one after it.
inline constexpr uint32_t MAX_TICKS_PER_FRAME = 4;

/// Converts elapsed real time into a whole number of fixed ticks.
///
/// The caller measures elapsed time and passes it in; this class never reads
/// a clock, so it is exact under test and the simulation never sees wall
/// time at all — only the tick count this returns.
///
/// Time is kept in nanoseconds multiplied by the tick rate, so one tick is
/// exactly 10^9 units and 1/60 s never rounds: sixty 16,666,667 ns frames
/// are sixty ticks, not fifty-nine with a remainder that drifts.
class FixedStepClock {
public:
  /// Accumulates `elapsed_ns` of real time and returns the ticks it pays for.
  FixedStepAdvance advance(uint64_t elapsed_ns);

private:
  /// Unspent time, in nanoseconds times `TICK_RATE_HZ`. Always below one
  /// tick after `advance` returns.
  uint64_t accumulator_ = 0;
};

}  // namespace eng
