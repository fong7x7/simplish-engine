#pragma once

// Design Summary -- PS5 Init & Lifecycle
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/init-lifecycle.md
//
// Behaviours:
//   - Initialise PS5 platform layer; validate SDK presence and version
//   - Return Ps5PlatformContext on success or nullopt on failure
//   - Tick PS5 system events once per frame (suspend/resume, network, user
//   service)
//   - Shut down all PS5 subsystems in reverse init order
//   - Query PS5 availability and local user ID
//
// Edge Cases:
//   - PS5_SDK_ROOT not set: init returns nullopt, engine continues in desktop
//   mode
//   - SDK version mismatch: init returns nullopt with descriptive log
//   - Tick/shutdown called when not initialised: safe no-op
//   - Double init: second call returns nullopt with warning
//
// Invariants:
//   - tickPs5() called exactly once per frame on main thread
//   - After shutdown, ps5Available() returns false
//   - No PS5 SDK types in public API
//
// Integration Points:
//   - Engine main loop: tickPs5() once per frame
//   - Engine init/shutdown: initPs5Platform() / shutdownPs5Platform()
//   - EventBus: Ps5PlatformContext holds non-owning EventBus* for event
//   emission
//   - Backend registration: init creates and registers GnmRhiBackend,
//   TempestAudioBackend, Ps5InputBackend

#include "ps5-platform-config.h"
#include "ps5-platform-context.h"
#include "ps5-types.h"

#include <optional>

namespace eng {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise the PS5 platform layer. Must be called early in engine startup,
/// before the first frame. Returns nullopt if PS5 SDK is not available,
/// version is mismatched, or init fails. Engine continues in desktop mode.
/// Main thread only.
std::optional<Ps5PlatformContext>
initPs5Platform(const Ps5PlatformConfig& config);

/// Tick PS5 system events. Call exactly once per frame from the main loop.
/// Processes suspend/resume, network reachability, and user service events.
/// No-op if PS5 is not initialised.
/// Main thread only.
void tickPs5(Ps5PlatformContext& ctx);

/// Shut down all PS5 subsystems in reverse init order. Flushes pending
/// save data. Safe to call on an uninitialised or already-shut-down context.
/// Main thread only.
void shutdownPs5Platform(Ps5PlatformContext& ctx);

/// Returns true if PS5 platform was successfully initialised and is active.
/// Thread-safe (reads an atomic flag).
bool ps5Available();

/// Returns the local user's PS5 user ID. Valid only when ps5Available().
/// Main thread only.
Ps5UserId ps5LocalUserId(const Ps5PlatformContext& ctx);

}  // namespace eng
