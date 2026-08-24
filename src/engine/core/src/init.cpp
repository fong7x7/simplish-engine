#include <array>
#include <engine/core/engine-context.h>
#include <engine/core/init.h>
#include <engine/core/logger.h>
#include <filesystem>
#include <memory>
#include <utility>

namespace eng {

Initializer::Initializer(EngineConfig config) : config_(std::move(config)) {}

namespace {

  /// Validate that a directory path exists or can be created.
  bool validateDir(std::string_view path) {
    if (path.empty()) {
      return false;
    }
    std::error_code ec;
    return std::filesystem::exists(path, ec) ||
           std::filesystem::create_directories(path, ec);
  }

  /// Log phase error and signal failure.
  bool phaseFailed(const std::optional<std::string>& error) {
    if (!error.has_value()) {
      return false;
    }
    Logger::error("Init", *error);
    return true;
  }

  using PhaseFunc = std::optional<std::string> (Initializer::*)(InitContext&);

  /// Phases 0–11: all take (InitContext&) with no extra arguments.
  constexpr std::array<PhaseFunc, 12> STANDARD_PHASES = {
      &Initializer::initFoundation,
      &Initializer::initCoreData,
      &Initializer::initWorldStorage,
      &Initializer::initVoxelSystem,
      &Initializer::initPhysics,
      &Initializer::initRendering,
      &Initializer::initAudio,
      &Initializer::initInput,
      &Initializer::initGui,
      &Initializer::initNetworking,
      &Initializer::initDayNightWeather,
      &Initializer::initTriggers,
  };

}  // namespace

std::optional<std::string> Initializer::initFoundation(InitContext& ctx) {
  Logger::info("Init", "phase 0: foundation");
  ctx.foundation_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initCoreData(InitContext& ctx) {
  Logger::info("Init", "phase 1: core data");
  if (!validateDir(ctx.config.data_dir)) {
    return "data_dir not accessible: " + ctx.config.data_dir;
  }
  ctx.core_data_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initWorldStorage(InitContext& ctx) {
  Logger::info("Init", "phase 2: world storage");
  ctx.world_storage_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initVoxelSystem(InitContext& ctx) {
  Logger::info("Init", "phase 3: voxel system");
  ctx.voxel_system_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initPhysics(InitContext& ctx) {
  Logger::info("Init", "phase 4: physics");
  ctx.physics_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initRendering(InitContext& ctx) {
  Logger::info("Init", "phase 5: rendering");
  ctx.rendering_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initAudio(InitContext& ctx) {
  Logger::info("Init", "phase 6: audio");
  ctx.audio_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initInput(InitContext& ctx) {
  Logger::info("Init", "phase 7: input");
  ctx.input_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initGui(InitContext& ctx) {
  Logger::info("Init", "phase 8: gui");
  ctx.gui_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initNetworking(InitContext& ctx) {
  Logger::info("Init", "phase 9: networking");
  ctx.networking_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initDayNightWeather(InitContext& ctx) {
  Logger::info("Init", "phase 10: day/night");
  ctx.day_night_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initTriggers(InitContext& ctx) {
  Logger::info("Init", "phase 11: triggers");
  ctx.triggers_ready = true;
  return std::nullopt;
}

std::optional<std::string>
Initializer::initModsPlugins(InitContext& ctx, PluginAPI* /*game_plugin_api*/) {
  Logger::info("Init", "phase 12: mods/plugins");
  ctx.plugins_ready = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initWorldLoad(InitContext& ctx) {
  Logger::info("Init", "phase 13: world load");
  ctx.world_loaded = true;
  return std::nullopt;
}

std::optional<std::string> Initializer::initSimulationStart(InitContext& ctx) {
  Logger::info("Init", "phase 14: simulation start");
  ctx.simulation_ready = true;
  return std::nullopt;
}

// Algorithm: Run STANDARD_PHASES then mods, world load, simulation start.
std::optional<std::unique_ptr<EngineContext>>
Initializer::initAll(PluginAPI* game_plugin_api) {
  InitContext ctx;
  ctx.config = config_;
  for (auto phase : STANDARD_PHASES) {
    if (phaseFailed((this->*phase)(ctx))) {
      return std::nullopt;
    }
  }
  if (phaseFailed(initModsPlugins(ctx, game_plugin_api))) {
    return std::nullopt;
  }
  if (phaseFailed(initWorldLoad(ctx))) {
    return std::nullopt;
  }
  if (phaseFailed(initSimulationStart(ctx))) {
    return std::nullopt;
  }
  auto engine_ctx = std::make_unique<EngineContext>();
  engine_ctx->config = config_;
  return engine_ctx;
}

void Initializer::shutdownAll(EngineContext& /*ctx*/) {
  Logger::info("Init", "shutting down all subsystems");
}

}  // namespace eng
