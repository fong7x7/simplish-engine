#include <cstddef>
#include <engine/core/frame-timings-config-loader.h>
#include <engine/core/logger.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace eng {

namespace {

  /// Read a JSON unsigned integer into `out` if the key exists and is valid.
  void readSize(const nlohmann::json& j, const char* key, std::size_t& out) {
    if (j.contains(key) && j[key].is_number_unsigned()) {
      out = j[key].get<std::size_t>();
    }
  }

  /// Read a JSON boolean into `out` if the key exists and is boolean.
  void readBool(const nlohmann::json& j, const char* key, bool& out) {
    if (j.contains(key) && j[key].is_boolean()) {
      out = j[key].get<bool>();
    }
  }

  /// Apply all recognised fields from the JSON object onto the config.
  void parseFields(const nlohmann::json& j, FrameTimingsConfig& cfg) {
    readSize(j, "ring_capacity", cfg.ring_capacity);
    readBool(j, "enabled", cfg.enabled);
  }

}  // namespace

FrameTimingsConfig loadFrameTimingsConfig(std::string_view path) {
  FrameTimingsConfig cfg;
  std::ifstream file{std::string(path)};
  if (!file.is_open()) {
    Logger::info("FrameTimingsConfig", "config not found; using defaults");
    return cfg;
  }

  nlohmann::json j =
      nlohmann::json::parse(file, /*cb=*/nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) {
    Logger::warn("FrameTimingsConfig",
                 "failed to parse config; using defaults");
    return cfg;
  }

  parseFields(j, cfg);
  return cfg;
}

}  // namespace eng
