#include <algorithm>
#include <engine/core/fixed-step-clock.h>

namespace eng {

namespace {

  /// Accumulator units in one tick: one second of nanoseconds, because the
  /// accumulator counts nanoseconds times the tick rate.
  constexpr uint64_t UNITS_PER_TICK = 1'000'000'000ULL;

}  // namespace

FixedStepAdvance FixedStepClock::advance(uint64_t elapsed_ns) {
  accumulator_ += elapsed_ns * TICK_RATE_HZ;
  const uint64_t due = accumulator_ / UNITS_PER_TICK;
  accumulator_ %= UNITS_PER_TICK;

  FixedStepAdvance result;
  result.ticks =
      static_cast<uint32_t>(std::min<uint64_t>(due, MAX_TICKS_PER_FRAME));
  result.dropped_ticks = due - result.ticks;
  result.interpolation = static_cast<float>(
      static_cast<double>(accumulator_) / static_cast<double>(UNITS_PER_TICK));
  return result;
}

}  // namespace eng
