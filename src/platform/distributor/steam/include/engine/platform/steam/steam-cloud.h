#pragma once

// Design Summary -- Steam Cloud Storage
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/cloud-storage.md
//
// Behaviours:
//   - Write a file to Steam Cloud (manual API for small config files)
//   - Read a file from Steam Cloud into an owned byte buffer
//   - Delete a file from Steam Cloud
//   - Query whether Steam Cloud is enabled for the account and app
//
// Edge Cases:
//   - Steam not available: all operations return SteamError::NOT_AVAILABLE
//   - Cloud disabled by user: returns SteamError::CLOUD_DISABLED
//   - File not found on read: returns SteamError::CLOUD_FILE_NOT_FOUND
//   - Write exceeds quota: returns SteamError::CLOUD_WRITE_FAILED
//   - Empty data or invalid filename: returns SteamError::INVALID_ARGUMENT
//
// Invariants:
//   - Manual API is for small config files only; Auto-Cloud handles world saves
//   - Cloud operations are synchronous
//   - Main thread only
//
// Integration Points:
//   - World Storage: Auto-Cloud handles .vxworld (no engine code)
//   - Config system: manual API syncs preferences.json, mods.toml

#include "steam-types.h"

#include <cstddef>
#include <engine/core/expected-polyfill.h>
#include <span>
#include <string_view>
#include <vector>

namespace eng {

// Forward declaration
struct SteamContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Write data to a Steam Cloud file. For small config files only
/// (preferences, mod load order). World saves use Auto-Cloud.
/// Main thread only.
std::expected<bool, SteamError> cloudWrite(const SteamContext& ctx,
                                           std::string_view filename,
                                           std::span<const std::byte> data);

/// Read a file from Steam Cloud into an owned byte buffer.
/// Returns the file contents or an error if the file does not exist
/// or Cloud is unavailable.
/// Main thread only.
std::expected<std::vector<std::byte>, SteamError>
cloudRead(const SteamContext& ctx, std::string_view filename);

/// Delete a file from Steam Cloud.
/// Main thread only.
std::expected<bool, SteamError> cloudDelete(const SteamContext& ctx,
                                            std::string_view filename);

/// Returns true if Steam Cloud is enabled for the current account and app.
/// Returns false if Steam is unavailable or Cloud is disabled.
/// Main thread only.
bool cloudIsEnabled(const SteamContext& ctx);

}  // namespace eng
