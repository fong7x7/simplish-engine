#pragma once

#include <cstdint>

namespace eng {

/// Start time and duration of one phase within a single engine tick.
///
/// Times are nanoseconds since the steady-clock epoch, captured by
/// `ScopedFrameTimer`. Both fields are zero for phases that did not run
/// in a given frame (e.g. when frame timing is disabled at compile time
/// via `ENGINE_FRAME_TIMING == 0`, or the corresponding phase is absent
/// in headless builds such as render submission).
///
/// Thread Safety: Plain value type. Writes happen on the main thread
/// between `FrameTimingsHistory::beginFrame` and `endFrame`. Readers
/// must obtain an immutable copy via `FrameTimingsHistory::snapshot`.
struct PhaseTimings {
  /// Steady-clock nanoseconds at phase entry. Zero if not measured.
  uint64_t start_ns = 0;
  /// Nanoseconds elapsed between phase entry and exit. Zero if not measured.
  uint64_t duration_ns = 0;
};

}  // namespace eng
