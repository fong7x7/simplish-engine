#pragma once

// Design Summary -- Steam Init & Lifecycle
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/init-lifecycle.md
//
// Behaviours:
//   - Initialise Steamworks SDK via SteamAPI_Init(); return SteamContext or
//   nullopt
//   - Dispatch pending Steam callbacks once per frame via tickSteamCallbacks()
//   - Shut down SDK and flush pending data via shutdownSteam()
//   - Query Steam availability, local user ID, display name, language
//   - Soft ownership check via SteamAPI_RestartAppIfNecessary() in shipping
//   builds
//
// Edge Cases:
//   - Steam client not running: init returns nullopt, engine continues
//   - Invalid App ID: init returns nullopt
//   - Tick/shutdown called when not initialised: safe no-op
//   - RestartAppIfNecessary returns true: engine should exit for Steam relaunch
//   - Double init: second call returns nullopt with warning
//
// Invariants:
//   - SteamAPI_RunCallbacks() called exactly once per frame on main thread
//   - After shutdown, steamAvailable() returns false
//   - No Steamworks SDK types in public API
//
// Integration Points:
//   - Engine main loop: tickSteamCallbacks() once per frame
//   - Engine init/shutdown: initSteam() / shutdownSteam()
//   - EventBus: SteamContext holds non-owning EventBus* for event emission

#include "steam-config.h"
#include "steam-context.h"
#include "steam-types.h"

#include <optional>
#include <string_view>

namespace eng {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise the Steamworks SDK. Must be called early in engine startup,
/// before the first frame. Returns nullopt if Steam client is not running,
/// App ID is invalid, or SDK init fails. Engine continues in non-Steam mode.
/// Main thread only.
std::optional<SteamContext> initSteam(const SteamConfig& config);

/// Dispatch all pending Steam callbacks. Call exactly once per frame
/// from the main loop. No-op if Steam is not initialised.
/// Main thread only.
void tickSteamCallbacks(const SteamContext& ctx);

/// Shut down the Steamworks SDK. Flushes pending cloud writes and stats.
/// Safe to call on an uninitialised or already-shut-down context.
/// Main thread only.
void shutdownSteam(SteamContext& ctx);

/// Returns true if Steam was successfully initialised and is active.
/// Thread-safe (reads an atomic flag).
bool steamAvailable();

/// Returns the local user's Steam ID. Valid only when steamAvailable().
/// Main thread only.
SteamUserId steamLocalUserId(const SteamContext& ctx);

/// Returns the local user's Steam display name. Valid only when
/// steamAvailable(). The returned view is valid until shutdownSteam().
/// Main thread only.
std::string_view steamLocalUserName(const SteamContext& ctx);

/// Returns the Steam client's current language as an API language code
/// (e.g. "english", "french"). Valid only when steamAvailable().
/// Main thread only.
std::string_view steamLanguage(const SteamContext& ctx);

}  // namespace eng
