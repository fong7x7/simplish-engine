#pragma once

#include "frame-timings-config.h"
#include "frame-timings.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eng {

/// Ring buffer of recent `FrameTimings` records, sized from
/// `FrameTimingsConfig` and owned by `Engine`.
///
/// Writers call `beginFrame(frame_number)` to acquire a scratch record for
/// the in-progress tick, fill its phase durations via `ScopedFrameTimer`,
/// then call `endFrame` to commit the scratch into the ring. Consumers call
/// `snapshot()` to obtain an independent, oldest-to-newest copy.
///
/// The ring is pre-allocated in the constructor; `beginFrame` / `endFrame`
/// perform no heap allocation. `snapshot()` allocates a single vector copy.
///
/// Thread Safety:
/// - `beginFrame`, `endFrame`: main thread only. Enforced by
///   `ThreadContext::assertMainThread`.
/// - `snapshot`: main thread OR any thread that is not concurrent with a
///   tick. The common pattern is a main-thread caller (e.g. the Profiler
///   panel) reading between ticks and handing the returned copy off to
///   whichever thread consumes it.
/// - `capacity`, `size`, `enabled`: const, lock-free, safe to call from any
///   thread.
class FrameTimingsHistory {
public:
  /// Construct a history sized and configured according to `config`.
  /// Clamps `ring_capacity` to `MIN_FRAME_TIMINGS_RING_CAPACITY` if zero.
  explicit FrameTimingsHistory(const FrameTimingsConfig& config);

  /// Begin a new frame with the given monotonic frame number. Returns a
  /// reference to the scratch slot that `ScopedFrameTimer` instances will
  /// write into during this tick. Calling `beginFrame` twice without an
  /// intervening `endFrame` asserts in debug (the previous scratch is
  /// discarded). When `enabled()` is false, returns a reference to a
  /// persistent sentinel slot and records nothing.
  FrameTimings& beginFrame(uint64_t frame_number);

  /// Commit the in-progress scratch frame into the ring. Advances the write
  /// cursor and records the total frame duration. Asserts if called without
  /// a matching `beginFrame`. No-op when `enabled()` is false.
  void endFrame();

  /// Immutable oldest-to-newest copy of all currently-retained frames.
  /// The returned vector size is `size()` and the elements are independent
  /// of the ring's subsequent mutation.
  std::vector<FrameTimings> snapshot() const;

  /// Maximum number of frames the ring holds before it begins evicting.
  std::size_t capacity() const;

  /// Number of frames currently committed (grows up to `capacity()`).
  std::size_t size() const;

  /// Runtime enable flag from `FrameTimingsConfig`. Orthogonal to the
  /// compile-time `ENGINE_FRAME_TIMING` macro.
  bool enabled() const;

  FrameTimingsHistory(const FrameTimingsHistory&) = delete;
  FrameTimingsHistory& operator=(const FrameTimingsHistory&) = delete;
  FrameTimingsHistory(FrameTimingsHistory&&) = delete;
  FrameTimingsHistory& operator=(FrameTimingsHistory&&) = delete;

private:
  /// Pre-allocated ring storage. `capacity_` entries, indexed modulo.
  std::vector<FrameTimings> ring_;
  /// Scratch slot for the in-progress frame. Copied into `ring_` on
  /// `endFrame`. Kept separate from the ring so that partial mid-tick
  /// snapshots never see torn data.
  FrameTimings scratch_;
  /// Sentinel returned by `beginFrame` when `enabled_` is false, so that
  /// callers using `ScopedFrameTimer` can still compile and run without
  /// contaminating real history data.
  FrameTimings disabled_slot_;
  /// Configured ring capacity (post-clamp). Mirrored on the class to avoid
  /// re-reading the config at read time.
  std::size_t capacity_ = 0;
  /// Next ring slot to write; wraps modulo `capacity_`.
  std::size_t write_index_ = 0;
  /// Count of committed frames; saturates at `capacity_`.
  std::size_t count_ = 0;
  /// Whether a `beginFrame` is currently open and awaiting `endFrame`.
  bool in_frame_ = false;
  /// Runtime enable flag (see `FrameTimingsConfig::enabled`).
  bool enabled_ = true;
};

}  // namespace eng
