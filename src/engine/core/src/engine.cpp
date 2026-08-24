#include <engine/core/engine-context.h>
#include <engine/core/engine.h>
#include <engine/core/frame-timings-config-loader.h>
#include <engine/core/frame-timings-config.h>
#include <engine/core/frame-timings-history.h>
#include <engine/core/init.h>
#include <engine/core/logger.h>
#include <engine/core/scoped-frame-timer.h>
#include <memory>
#include <string>

namespace eng {

namespace {

  /// Resolve the frame-timing JSON config path relative to the engine data dir.
  /// Empty `data_dir` loads defaults via the loader's missing-file fallback.
  std::string frameTimingConfigPath(const EngineConfig& config) {
    if (config.data_dir.empty()) {
      return "frame-timing.json";
    }
    return config.data_dir + "/config/frame-timing.json";
  }

}  // namespace

Engine::Engine(std::unique_ptr<EngineContext> ctx) : context_(std::move(ctx)) {
  const FrameTimingsConfig timing_config =
      loadFrameTimingsConfig(frameTimingConfigPath(context_->config));
  frame_timings_history_ = std::make_unique<FrameTimingsHistory>(timing_config);
}

Engine::~Engine() {
  Logger::info("Engine", "shutting down");
}

std::optional<std::unique_ptr<Engine>>
Engine::create(const EngineConfig& config, PluginAPI* game_plugin_api) {
  Initializer init{config};
  auto result = init.initAll(game_plugin_api);
  if (!result.has_value() || *result == nullptr) {
    return std::nullopt;
  }
  // Private ctor: std::make_unique cannot be instantiated outside Engine
  // members.
  return std::unique_ptr<Engine>(
      new Engine(std::move(*result)));  // NOLINT(bare-new-delete)
}

bool Engine::tick(float /*delta_time_seconds*/) {
  FrameTimings& frame = frame_timings_history_->beginFrame(tick_count_);
  tickChunkIntegration(frame);
  tickPhysicsStep(frame);
  tickVoxelSystem(frame);
  tickEntitySystem(frame);
  tickAnimationSystem(frame);
  tickPluginSync(frame);
  tickEventDispatch(frame);
  tickRenderSubmit(frame);
  frame_timings_history_->endFrame();
  ++tick_count_;
  return true;
}

void Engine::tickChunkIntegration(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.chunk_integration);
  // TODO: integrate I/O-loaded chunks once ChunkManager supports it.
}

void Engine::tickPhysicsStep(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.physics_step);
  // TODO: call PhysicsWorld::step once physics lifecycle is wired.
}

void Engine::tickVoxelSystem(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.voxel_system);
  // TODO: call VoxelSystem::update once implemented.
}

void Engine::tickEntitySystem(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.entity_system);
  // TODO: call EntitySystem::update once implemented.
}

void Engine::tickAnimationSystem(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.animation_system);
  // TODO: call AnimationSystem::update once implemented.
}

void Engine::tickPluginSync(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.plugin_sync);
  // TODO: invoke main-thread plugin tick callbacks.
}

void Engine::tickEventDispatch(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.event_dispatch);
  // Events dispatch synchronously inside `emit` (ADR-004); this slot
  // reserves timing space for any future queued-event drains.
}

void Engine::tickRenderSubmit(FrameTimings& frame) {
  ENG_FRAME_PHASE_TIMER(frame.render_submit);
  // TODO: hand off render commands to the render thread.
}

void Engine::flushDirtyChunks() {
  Logger::debug("Engine", "flushing dirty chunks");
}

void Engine::saveWorld() {
  Logger::info("Engine", "saving world");
}

void Engine::shutdownPlugins() {
  Logger::info("Engine", "shutting down plugins");
}

VoxelSystem* Engine::voxelSystem() const {
  return context_->voxel_system;
}
PhysicsWorld* Engine::physicsWorld() const {
  return context_->physics_world;
}
RhiDevice* Engine::rhiDevice() const {
  return context_->rhi_device;
}
RenderPipeline* Engine::renderPipeline() const {
  return context_->render_pipeline;
}
IAudioBackend* Engine::audioBackend() const {
  return context_->audio_backend;
}
EventBus* Engine::eventBus() const {
  return context_->event_bus;
}
InputSystem* Engine::inputSystem() const {
  return context_->input_system;
}
GuiSystem* Engine::guiSystem() const {
  return context_->gui_system;
}
uint64_t Engine::tickCount() const {
  return tick_count_;
}
uint64_t Engine::deterministicSeed() const {
  return context_->config.world_seed;
}

const EngineContext& Engine::context() const {
  return *context_;
}

const FrameTimingsHistory& Engine::frameTimingsHistory() const {
  return *frame_timings_history_;
}

}  // namespace eng
