#pragma once

#include "engine-config.h"
#include "engine-context.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// InitContext: Tracks the state of engine initialization across 15 phases.
//
// Each boolean flag indicates whether a phase completed successfully.
// Presentation phases (rendering, audio, input, GUI) are set to true
// automatically in Headless mode.
//
// Thread Safety:
// - Main thread only (used only during init sequence).
// ============================================================================

struct InitContext {
  /// Engine configuration used for this initialization sequence.
  EngineConfig config;
  /// Engine context being built up across init phases.
  std::unique_ptr<EngineContext> engine{};

  /// Whether the foundation phase (logging, platform, config) completed.
  bool foundation_ready = false;
  /// Whether core data (voxel registry, expressions, etc.) loaded.
  bool core_data_ready = false;
  /// Whether world storage (SQLite, region files) initialized.
  bool world_storage_ready = false;
  /// Whether the voxel system (queries, structural integrity) initialized.
  bool voxel_system_ready = false;
  /// Whether physics and animation initialized.
  bool physics_ready = false;
  /// Whether rendering initialized (true in Headless, skipped).
  bool rendering_ready = false;
  /// Whether audio initialized (true in Headless, skipped).
  bool audio_ready = false;
  /// Whether input initialized (true in Headless, skipped).
  bool input_ready = false;
  /// Whether GUI initialized (true in Headless, skipped).
  bool gui_ready = false;
  /// Whether networking initialized.
  bool networking_ready = false;
  /// Whether day/night and weather systems initialized.
  bool day_night_ready = false;
  /// Whether trigger systems initialized.
  bool triggers_ready = false;
  /// Whether mods and plugins loaded.
  bool plugins_ready = false;
  /// Whether world data (chunks, entities, scenario) loaded.
  bool world_loaded = false;
  /// Whether the simulation is ready to tick.
  bool simulation_ready = false;

  /// Ordered log of init phase messages for debugging.
  std::vector<std::string> init_log{};

  // Query phase completion by name
  bool isPhaseReady(std::string_view phase_name) const;
};

// Phase name constants
inline constexpr std::string_view PHASE_FOUNDATION = "foundation";
inline constexpr std::string_view PHASE_CORE_DATA = "core_data";
inline constexpr std::string_view PHASE_WORLD_STORAGE = "world_storage";
inline constexpr std::string_view PHASE_VOXEL_SYSTEM = "voxel_system";
inline constexpr std::string_view PHASE_PHYSICS = "physics";
inline constexpr std::string_view PHASE_RENDERING = "rendering";
inline constexpr std::string_view PHASE_AUDIO = "audio";
inline constexpr std::string_view PHASE_INPUT = "input";
inline constexpr std::string_view PHASE_GUI = "gui";
inline constexpr std::string_view PHASE_NETWORKING = "networking";
inline constexpr std::string_view PHASE_DAY_NIGHT = "day_night";
inline constexpr std::string_view PHASE_TRIGGERS = "triggers";
inline constexpr std::string_view PHASE_PLUGINS = "plugins";
inline constexpr std::string_view PHASE_WORLD_LOAD = "world_load";
inline constexpr std::string_view PHASE_SIMULATION = "simulation";

}  // namespace eng
