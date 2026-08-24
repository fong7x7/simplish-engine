#pragma once

#include "phase-timings.h"

#include <cstdint>

namespace eng {

/// Per-tick timing capture for the eight canonical phases of `Engine::tick`.
///
/// The phase list matches `docs/technical-approaches/engine/data-flow.md`.
/// `FrameTimings` is a fixed-shape POD — see ADR-015 for rationale — and is
/// stored inside a `FrameTimingsHistory` ring buffer. Each phase is measured
/// by an RAII `ScopedFrameTimer` instantiated at the phase boundary.
///
/// Thread Safety: Written on the main thread (inside `Engine::tick`) between
/// `FrameTimingsHistory::beginFrame` and `endFrame`. Read-only access off the
/// main thread must go through `FrameTimingsHistory::snapshot`, which returns
/// an independent copy.
struct FrameTimings {
  /// Monotonic tick index at which this frame was captured.
  uint64_t frame_number = 0;
  /// Steady-clock nanoseconds at frame start (call to `beginFrame`).
  uint64_t frame_start_ns = 0;
  /// Nanoseconds from `beginFrame` to `endFrame`.
  uint64_t frame_duration_ns = 0;
  /// Phase 1 — merging I/O-loaded chunks into the live world.
  PhaseTimings chunk_integration{};
  /// Phase 2 — Jolt deterministic physics step.
  PhaseTimings physics_step{};
  /// Phase 3 — voxel system update (structural integrity, voxel events).
  PhaseTimings voxel_system{};
  /// Phase 4 — entity system update (component ticks, entity spawn/despawn).
  PhaseTimings entity_system{};
  /// Phase 5 — animation system update (ozz skeletal animation, blending).
  PhaseTimings animation_system{};
  /// Phase 6 — synchronous plugin tick callbacks on the main thread.
  PhaseTimings plugin_sync{};
  /// Phase 7 — synchronous event-bus dispatch of queued events.
  PhaseTimings event_dispatch{};
  /// Phase 8 — render pipeline submission hand-off to the render thread.
  PhaseTimings render_submit{};
};

}  // namespace eng
