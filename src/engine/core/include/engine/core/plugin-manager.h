#pragma once

#include "loaded-plugin.h"
#include "mod-manifest.h"
#include "plugin-api.h"

#include <optional>
#include <string_view>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// PluginManager: Load and unload C++ plugins from mods via C ABI.
//
// Responsibilities:
// - Discover mods on disk from manifest.json files
// - Resolve mod dependencies in topological order
// - Check plugin API version compatibility before loading
// - For each mod with a plugin field: dlopen, simplishPluginInit, track
// - Handle init failures gracefully (log, skip plugin, continue)
// - Manage async tick loops (dedicated threads per plugin)
// - Unload plugins in reverse order at shutdown
//
// Key Invariants:
// - Mods loaded in dependency order (manifest.json specifies dependencies)
// - Plugin API version checked before calling init (ABI safety)
// - Plugin init returns bool; false = failed, mark unhealthy
// - dlopen failures caught, logged, plugin skipped
// - Plugins loaded once per engine instance
// - Unload is deterministic (reverse order)
// - Async tick loops stopped before plugin shutdown
// - Game layer provides PluginAPI*; manager does not construct it
//
// Thread Safety:
// - loadMods: main thread only (during init)
// - unloadPlugins: main thread only (during shutdown)
// - stopAsyncTickLoops: main thread only (before plugin shutdown)
// ============================================================================

class PluginManager {
public:
  // Load mods in dependency order; check API version; dlopen; call init
  std::optional<std::vector<LoadedPlugin>>
  loadMods(std::string_view mods_dir,
           const std::vector<CoreModManifest>& manifests,
           PluginAPI* game_plugin_api);

  // Stop all async tick loops (join dedicated threads)
  void stopAsyncTickLoops();

  // Unload all plugins in reverse load order
  void unloadPlugins();

  // Query loaded plugins (const)
  const std::vector<LoadedPlugin>& plugins() const;

  // Mutable access to loaded plugins (for hot-reload)
  std::vector<LoadedPlugin>& mutablePlugins();

  // Mark plugin unhealthy (subsequent calls skipped)
  void markUnhealthy(std::string_view mod_id);

private:
  /// Plugins loaded in dependency order (unloaded in reverse).
  std::vector<LoadedPlugin> loaded_plugins_;
};

}  // namespace eng
