#pragma once

#include "loaded-plugin.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// PluginBinaryWatcher: Poll-based file watcher for plugin shared libraries.
//
// Monitors plugin binary files for modification time changes. On each poll,
// returns the list of mod_ids whose binaries have been modified since the
// last check. Used by the hot-reload system to detect when a developer
// rebuilds a plugin.
//
// Dev-mode only (ENGINE_DEV_MODE). Disabled in shipping builds.
//
// Thread Safety:
// - Main thread only. All calls must happen on the main thread.
// ============================================================================

/// Poll-based watcher for plugin shared library file changes.
/// Thread safety: main thread only.
struct PluginBinaryWatcher {
  /// A single watched plugin binary with its last known modification time.
  struct WatchedBinary {
    /// Mod ID this binary belongs to.
    std::string mod_id;
    /// Absolute path to the shared library file.
    std::string binary_path;
    /// Last modification time in nanoseconds since epoch.
    int64_t last_mtime_ns = 0;
  };

  /// Configuration for creating a plugin binary watcher.
  struct Config {
    /// Root mods directory to resolve relative plugin paths.
    std::string_view mods_directory;
  };
  /// Binaries currently being watched.
  std::vector<WatchedBinary> watched_binaries{};

  /// Create a watcher from the currently loaded plugins.
  /// Resolves platform-specific binary paths and records initial mtimes.
  static PluginBinaryWatcher create(const Config& config,
                                    const std::vector<LoadedPlugin>& plugins);

  /// Poll for changed binaries. Returns mod_ids whose binaries have been
  /// modified since the last poll.
  static std::vector<std::string> poll(PluginBinaryWatcher& watcher);

  /// Get the current modification time of a file in nanoseconds.
  /// Returns 0 if the file does not exist or cannot be stat'd.
  static int64_t getFileMtimeNs(std::string_view path);
};

}  // namespace eng
