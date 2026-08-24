#pragma once

#include <cstddef>

namespace eng {

/// Runtime configuration for the engine's per-phase frame-timing ring buffer.
///
/// Loaded from `data/config/frame-timing.json` by
/// `loadFrameTimingsConfig`. Sensible defaults are used when the file is
/// missing or unparseable, matching the pattern used by other engine config
/// loaders.
///
/// Thread Safety: Read-only after engine construction. Safe to read from any
/// thread; never mutated after init.
struct FrameTimingsConfig {
  /// Number of past frames retained in the ring buffer. Oldest frames are
  /// overwritten when the ring is full. 600 ≈ 10 seconds at 60 Hz.
  std::size_t ring_capacity = 600;
  /// When false, `FrameTimingsHistory::beginFrame` / `endFrame` are no-ops
  /// and `snapshot` returns an empty vector. Separate from the compile-time
  /// `ENGINE_FRAME_TIMING` macro: this toggles at runtime; the macro
  /// removes the code entirely.
  bool enabled = true;
};

/// Minimum permitted `ring_capacity`. A zero value from config is clamped up
/// to this with a warning log — the history is never left unusable.
inline constexpr std::size_t MIN_FRAME_TIMINGS_RING_CAPACITY = 1;

}  // namespace eng
