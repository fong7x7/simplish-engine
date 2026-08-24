#pragma once

#include "engine-config.h"
#include "engine-context.h"
#include "frame-timings-history.h"
#include "plugin-api.h"

#include <engine/core/event-bus.h>
#include <memory>
#include <optional>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Engine: High-level lifecycle and subsystem orchestration.
//
// Responsibilities:
// - Encapsulate engine-wide state (EngineContext with focused sub-contexts)
// - Manage subsystem initialization order (15 phases, dependency-ordered)
// - Coordinate simulation tick (main loop frame)
// - Provide subsystem access to game layer and editor
// - Manage shutdown sequence (reverse of init, 15 steps)
//
// Key Invariants:
// - Single Engine instance per process (enforced by unique_ptr in game layer)
// - Engine is game-agnostic (never #includes game/ or editor/)
// - No global singletons; engine passed explicitly via context
// - Error handling: All init failures return optional<error_msg>
// - Subsystems fully initialized before simulation starts
// - Shutdown flushes dirty chunks and saves before releasing resources
// - Presentation subsystems nullptr in HEADLESS mode (dedicated server)
//
// Threading:
// - Constructor/destructor on main thread only
// - tick() called once per frame on main thread
// - Render thread runs in parallel (via renderPipeline())
// - Workers and I/O threads managed internally by subsystems
// ============================================================================

// RAII wrapper for engine lifetime
class Engine {
public:
  // Construct and initialize the engine via Initializer (all 15 phases).
  // config: engine configuration
  // game_plugin_api: populated by game layer with engine + game domain
  // functions Returns: Engine instance if successful, nullopt if any init phase
  // fails
  static std::optional<std::unique_ptr<Engine>>
  create(const EngineConfig& config, PluginAPI* game_plugin_api);

  ~Engine();

  // Access subsystems (nullptr in HEADLESS for presentation subsystems)
  VoxelSystem* voxelSystem() const;
  PhysicsWorld* physicsWorld() const;
  RhiDevice* rhiDevice() const;            // nullptr in HEADLESS
  RenderPipeline* renderPipeline() const;  // nullptr in HEADLESS
  IAudioBackend* audioBackend() const;     // nullptr in HEADLESS
  EventBus* eventBus() const;
  InputSystem* inputSystem() const;  // nullptr in HEADLESS
  GuiSystem* guiSystem() const;      // nullptr in HEADLESS

  // Main game loop: tick simulation, physics, plugins, dispatch events.
  // Returns false if simulation should halt.
  bool tick(float delta_time_seconds);

  /// Read-only access to the per-phase frame-timing ring buffer. Consumers
  /// (Profiler panel) call `snapshot()` to obtain an immutable copy of the
  /// retained frames. Thread Safety: safe to call from any thread that is
  /// not concurrent with `tick`; see `FrameTimingsHistory::snapshot` for the
  /// full contract.
  const FrameTimingsHistory& frameTimingsHistory() const;

  // Shutdown helpers (called by game layer before destruction)
  void flushDirtyChunks();
  void saveWorld();
  void shutdownPlugins();

  // Determinism queries
  uint64_t tickCount() const;
  uint64_t deterministicSeed() const;

  // Query internal state (for editor/debugging)
  const EngineContext& context() const;

  // Prevent copying/moving
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

private:
  explicit Engine(std::unique_ptr<EngineContext> ctx);
  // Per-phase tick helpers. Each owns one `ScopedFrameTimer` and will grow
  // when the corresponding subsystem's per-frame call is implemented.
  void tickChunkIntegration(FrameTimings& frame);
  void tickPhysicsStep(FrameTimings& frame);
  void tickVoxelSystem(FrameTimings& frame);
  void tickEntitySystem(FrameTimings& frame);
  void tickAnimationSystem(FrameTimings& frame);
  void tickPluginSync(FrameTimings& frame);
  void tickEventDispatch(FrameTimings& frame);
  void tickRenderSubmit(FrameTimings& frame);

  /// Owns the engine-wide context containing all subsystem state.
  std::unique_ptr<EngineContext> context_{};
  /// Number of simulation ticks completed.
  uint64_t tick_count_ = 0;
  /// Ring buffer of recent per-phase frame timings. Lives for the engine
  /// lifetime; written by `tick`, read by editor/debug consumers.
  std::unique_ptr<FrameTimingsHistory> frame_timings_history_{};
};

}  // namespace eng
