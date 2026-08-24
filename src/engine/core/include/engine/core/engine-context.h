#pragma once

#include "engine-config.h"

#include <cstdint>
#include <engine/core/event-bus.h>

namespace eng {

// Forward declarations: subsystem types live in higher dependency layers
// (Layer 1–3) that core (Layer 0) cannot include. Raw non-owning pointers
// are used here; ownership resides with the code that creates each subsystem.
class VoxelSystem;     // NOLINT(no-forward-decl) avoids circular dependency
class PhysicsWorld;    // NOLINT(no-forward-decl) avoids circular dependency
class GuiSystem;       // NOLINT(no-forward-decl) avoids circular dependency
class RhiDevice;       // NOLINT(no-forward-decl) avoids circular dependency
class RenderPipeline;  // NOLINT(no-forward-decl) avoids circular dependency
class IAudioBackend;   // NOLINT(no-forward-decl) avoids circular dependency
class InputSystem;     // NOLINT(no-forward-decl) avoids circular dependency

/// Engine-wide context containing all subsystem state.
/// Passed to subsystems and game layer via Engine accessors.
///
/// Subsystem pointers are non-owning (raw). Ownership belongs to
/// the Initializer phases or subsystem modules that create them.
/// All pointers are nullptr until their corresponding init phase runs.
///
/// Thread Safety: Main-thread-only access unless explicitly noted.
struct EngineContext {
  /// Snapshot of the engine configuration used at init time.
  EngineConfig config;

  /// Monotonically increasing simulation tick counter.
  uint64_t tick_count = 0;

  // -- Subsystem pointers (non-owning, set during init phases) --

  /// Voxel queries, structural integrity (Layer 1 — World).
  VoxelSystem* voxel_system = nullptr;
  /// Physics simulation world (Layer 2 — Simulation).
  PhysicsWorld* physics_world = nullptr;
  /// Render hardware interface device (Layer 3 — Presentation).
  RhiDevice* rhi_device = nullptr;
  /// Render pipeline (Layer 3 — Presentation).
  RenderPipeline* render_pipeline = nullptr;
  /// Audio backend (Layer 3 — Presentation).
  IAudioBackend* audio_backend = nullptr;
  /// Event dispatch bus (Layer 0 — Foundation).
  EventBus* event_bus = nullptr;
  /// Input action maps and gamepad (Layer 3 — Presentation).
  InputSystem* input_system = nullptr;
  /// Retained-mode GUI system (Layer 3 — Presentation).
  GuiSystem* gui_system = nullptr;
};

}  // namespace eng
