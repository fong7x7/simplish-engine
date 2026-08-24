#pragma once

#include "phase-timings.h"

#include <cstdint>

namespace eng {

/// RAII timer that records the elapsed wall time of a single tick phase
/// into a `PhaseTimings` slot.
///
/// Construction captures the current steady-clock time into `target.start_ns`;
/// destruction writes the elapsed duration into `target.duration_ns`.
///
/// When the feature macro `ENGINE_FRAME_TIMING == 0` is defined at compile
/// time, the class has no members and both ctor and dtor are empty inline
/// bodies — the optimiser removes all runtime cost, including the clock
/// reads and the store.
///
/// Thread Safety: Call from the main thread inside `Engine::tick`. The
/// targeted `PhaseTimings` must live through the timer's scope and must not
/// be concurrently written by any other thread.
class ScopedFrameTimer {
public:
  /// Start the timer and record `target.start_ns`.
  explicit ScopedFrameTimer(PhaseTimings& target);
  /// Stop the timer and record `target.duration_ns`.
  ~ScopedFrameTimer();

  ScopedFrameTimer(const ScopedFrameTimer&) = delete;
  ScopedFrameTimer& operator=(const ScopedFrameTimer&) = delete;
  ScopedFrameTimer(ScopedFrameTimer&&) = delete;
  ScopedFrameTimer& operator=(ScopedFrameTimer&&) = delete;

private:
#if !(defined(ENGINE_FRAME_TIMING) && ENGINE_FRAME_TIMING == 0)
  /// Target slot whose `start_ns` / `duration_ns` we populate. Non-owning.
  PhaseTimings* target_;
  /// Steady-clock nanoseconds captured at construction.
  uint64_t start_ns_;
#endif
};

/// Current steady-clock time, in nanoseconds since the steady-clock epoch.
/// Exposed as a free helper so tests can compare against the same source the
/// timer uses. Thread Safety: any thread.
uint64_t currentSteadyNanoseconds();

}  // namespace eng

#if defined(ENGINE_FRAME_TIMING) && ENGINE_FRAME_TIMING == 0

// NOLINTBEGIN(cppcoreguidelines-macro-usage,cppcoreguidelines-avoid-do-while)
// -- macros are the only way to produce a zero-overhead scoped declaration
//    that vanishes when the feature is disabled at compile time.
#define ENG_FRAME_PHASE_TIMER(phase_ref)                                       \
  do {                                                                         \
    (void)sizeof(phase_ref);                                                   \
  } while (false)
// NOLINTEND(cppcoreguidelines-macro-usage,cppcoreguidelines-avoid-do-while)

#else

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define ENG_FRAME_PHASE_TIMER_CONCAT_INNER(a, b) a##b
#define ENG_FRAME_PHASE_TIMER_CONCAT(a, b)                                     \
  ENG_FRAME_PHASE_TIMER_CONCAT_INNER(a, b)
#define ENG_FRAME_PHASE_TIMER(phase_ref)                                       \
  ::eng::ScopedFrameTimer ENG_FRAME_PHASE_TIMER_CONCAT(eng_frame_phase_timer_, \
                                                       __LINE__) {             \
    (phase_ref)                                                                \
  }
// NOLINTEND(cppcoreguidelines-macro-usage)

#endif
