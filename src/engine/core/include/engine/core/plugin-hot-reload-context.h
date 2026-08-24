#pragma once

#include "plugin-api.h"

#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// PluginHotReloadContext: Input context for a single plugin reload operation.
//
// Thread Safety:
// - Main thread only. Passed by const& to hotReloadPlugin.
// ============================================================================

/// Context for a single hot-reload operation.
/// Thread safety: main thread only.
struct PluginHotReloadContext {
  /// Mod ID of the plugin to reload.
  std::string_view mod_id;
  /// Absolute path to the new plugin binary.
  std::string_view binary_path;
  /// Plugin API to pass to simplishPluginInit on the new binary.
  PluginAPI* plugin_api;
};

}  // namespace eng
