#pragma once

#include "hot-reload-result.h"
#include "plugin-hot-reload-context.h"
#include "plugin-manager.h"

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Plugin Hot-Reload: Orchestrates unloading an old plugin binary and loading
// a new one during development.
//
// Sequence:
//   1. Find the LoadedPlugin by mod_id
//   2. Stop async tick loop (if active)
//   3. Look up optional simplishPluginSerializeState → call if present
//   4. Call simplishPluginShutdown on old binary
//   5. DynamicLibrary::close old handle
//   6. Validate new binary (dlopen, symbol resolution, API version check)
//   7. Call simplishPluginInit on new binary
//   8. Look up optional simplishPluginDeserializeState → call if present
//   9. Update LoadedPlugin with new dlhandle
//
// On any failure after step 4, the plugin is marked unhealthy and the
// engine continues without it.
//
// Dev-mode only (ENGINE_DEV_MODE).
//
// Thread Safety:
// - Main thread only.
// ============================================================================

/// Perform a hot-reload of a single plugin.
/// Returns the outcome; on failure the plugin is marked unhealthy.
/// Thread safety: main thread only.
HotReloadResult hotReloadPlugin(const PluginHotReloadContext& ctx,
                                PluginManager& manager);

/// Expected API version for the current engine build.
/// Plugins must report this exact version to pass validation.
/// Thread safety: compile-time constant.
inline constexpr uint32_t EXPECTED_PLUGIN_API_VERSION = 1;

}  // namespace eng
