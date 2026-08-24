#pragma once

#include <cstdint>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// HotReloadResult: Outcome of a plugin hot-reload attempt.
//
// Thread Safety:
// - Immutable value type — safe to copy across threads.
// ============================================================================

/// Outcome of a plugin hot-reload attempt.
enum class HotReloadResult : uint8_t {
  SUCCESS,               ///< Reload completed successfully
  BINARY_NOT_FOUND,      ///< Plugin binary file does not exist
  DLOPEN_FAILED,         ///< dlopen/LoadLibrary returned nullptr
  MISSING_SYMBOLS,       ///< Required entry point symbols not found
  API_VERSION_MISMATCH,  ///< Binary compiled against different API version
  INIT_FAILED,           ///< simplishPluginInit returned false
};

/// Convert a HotReloadResult to a human-readable string for logging.
/// Thread safety: stateless, safe from any thread.
const char* hotReloadResultToString(HotReloadResult result);

}  // namespace eng
