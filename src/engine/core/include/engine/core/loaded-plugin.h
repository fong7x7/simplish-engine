#pragma once

#include <atomic>
#include <string>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// LoadedPlugin: Runtime state for a plugin loaded via dlopen.
//
// Tracks the mod identity, dynamic library handle, async tick configuration,
// and health status. Owned by PluginManager in load-order sequence.
//
// Thread Safety:
// - is_healthy is atomic; other fields are read-only after load.
// ============================================================================

struct LoadedPlugin {
  /// Mod ID this plugin belongs to.
  std::string mod_id;
  /// Display name of the plugin (from simplishPluginName).
  std::string plugin_name;
  /// Relative path to the plugin binary within the mod directory.
  std::string plugin_path;
  /// Platform dynamic library handle returned by dlopen.
  void* dlhandle = nullptr;
  /// Whether this plugin registered an async tick loop.
  bool has_async_tick = false;
  /// Target tick rate in Hz for the async tick loop (0 if none).
  float async_tick_rate_hz = 0.0f;
  /// Whether the plugin is currently healthy (false after init failure).
  std::atomic<bool> is_healthy{true};

  LoadedPlugin() = default;
  LoadedPlugin(const LoadedPlugin&) = delete;
  LoadedPlugin& operator=(const LoadedPlugin&) = delete;
  /// Move constructor loads atomic health flag from source.
  LoadedPlugin(LoadedPlugin&& other) noexcept
    : mod_id(std::move(other.mod_id)),
      plugin_name(std::move(other.plugin_name)),
      plugin_path(std::move(other.plugin_path)), dlhandle(other.dlhandle),
      has_async_tick(other.has_async_tick),
      async_tick_rate_hz(other.async_tick_rate_hz),
      is_healthy(other.is_healthy.load()) {
    other.dlhandle = nullptr;
  }
  /// Move assignment loads atomic health flag from source.
  LoadedPlugin& operator=(LoadedPlugin&& other) noexcept;
};

}  // namespace eng
