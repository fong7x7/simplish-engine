#pragma once

#include "engine-context.h"
#include "init-context.h"
#include "plugin-api.h"

#include <memory>
#include <optional>
#include <string>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Initializer: Ordered initialization of engine subsystems (15 phases).
//
// Responsibilities:
// - Execute 15 init phases in dependency order
// - Track completion state in InitContext
// - Skip presentation phases (render, audio, input, GUI) in HEADLESS mode
// - Return error message if any phase fails
// - Provide shutdownAll() for reverse-order teardown (15 steps)
//
// Key Invariants:
// - Each phase has explicit dependencies (documented in REQUIREMENTS.md)
// - Phase order is immutable (enforced by design)
// - If a phase fails, subsequent phases are not attempted
// - Plugin init failure is non-fatal (skip plugin, continue)
// - World load failure is fatal (abort init)
// - Phases 5-8 (render, audio, input, GUI) skipped in HEADLESS
//
// Phase Order:
// 0. Foundation (logging, platform, config)
// 1. Core data (voxel registry, expressions, conditions, actions)
// 2. World storage (SQLite, region files, chunk manager)
// 3. Voxel system (voxel queries, structural integrity)
// 4. Physics & animation (Jolt, forces, particles, ozz-animation)
// 5. Rendering [SKIP in HEADLESS] (RHI device, pipeline)
// 6. Audio [SKIP in HEADLESS] (OpenAL/Tempest/XAudio2)
// 7. Input [SKIP in HEADLESS] (SDL3 action maps, gamepad)
// 8. GUI [SKIP in HEADLESS] (FreeType, HarfBuzz, retained-mode)
// 9. Networking (ENet optional)
// 10. Day/night & weather
// 11. Triggers (time, location, event-based)
// 12. Mods & plugins (API version check, dlopen, init, async ticks)
// 13. World load (chunks, entities, scenario)
// 14. Simulation start (tick_count=0, seed RNG, enable events)
// ============================================================================

class Initializer {
public:
  explicit Initializer(EngineConfig config);

  // Consolidated: runs all 15 phases; returns engine context or nullopt
  std::optional<std::unique_ptr<EngineContext>>
  initAll(PluginAPI* game_plugin_api);

  // Shutdown: reverse of init (15 steps)
  void shutdownAll(EngineContext& ctx);

  // Individual phase functions (for testing / granular control)
  std::optional<std::string> initFoundation(InitContext& ctx);
  std::optional<std::string> initCoreData(InitContext& ctx);
  std::optional<std::string> initWorldStorage(InitContext& ctx);
  std::optional<std::string> initVoxelSystem(InitContext& ctx);
  std::optional<std::string> initPhysics(InitContext& ctx);
  std::optional<std::string> initRendering(InitContext& ctx);
  std::optional<std::string> initAudio(InitContext& ctx);
  std::optional<std::string> initInput(InitContext& ctx);
  std::optional<std::string> initGui(InitContext& ctx);
  std::optional<std::string> initNetworking(InitContext& ctx);
  std::optional<std::string> initDayNightWeather(InitContext& ctx);
  std::optional<std::string> initTriggers(InitContext& ctx);
  std::optional<std::string> initModsPlugins(InitContext& ctx,
                                             PluginAPI* game_plugin_api);
  std::optional<std::string> initWorldLoad(InitContext& ctx);
  std::optional<std::string> initSimulationStart(InitContext& ctx);

private:
  /// Snapshot of the engine configuration for this initializer.
  EngineConfig config_;
};

}  // namespace eng
