#pragma once

/// @file fixed-step-advance.h
/// @brief What one frame's elapsed time buys the simulation.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng {

/// The result of `FixedStepClock::advance`: how many ticks to run this frame,
/// what was dropped to stay inside the clamp, and where rendering sits
/// between the last two simulated states.
struct FixedStepAdvance {
  /// Ticks to run now, never more than `MAX_TICKS_PER_FRAME`.
  uint32_t ticks = 0;
  /// Ticks the elapsed time was worth beyond the clamp. They are discarded,
  /// not carried, and reported so a stall is visible rather than absorbed
  /// (Engine REQUIREMENTS §4.1).
  uint64_t dropped_ticks = 0;
  /// Fraction of a tick left over, in `[0, 1)`. Rendering interpolates by
  /// it; nothing in the simulation may read it.
  float interpolation = 0.0F;
};

}  // namespace eng
