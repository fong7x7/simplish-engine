#pragma once

// Design Summary -- Xbox Series X Init & Lifecycle
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x/init-lifecycle.md
//
// Behaviours:
//   - Initialise GDK runtime; return XboxContext or nullopt on failure
//   - Register suspend/resume callbacks for PLM and Quick Resume
//   - Tick GDK task queue once per frame for async callback dispatch
//   - Manage user sign-in and sign-out events
//   - Query Xbox availability, local user ID, display name, lifecycle state
//   - Shut down GDK runtime cleanly
//
// Edge Cases:
//   - GDK runtime init failure: return nullopt, engine continues
//   - Suspend during active save: save must complete before PLM ack
//   - Resume after extended suspend (Quick Resume): re-validate sessions
//   - User signs out during gameplay: emit event via EventBus
//   - Double init: return nullopt with warning
//   - Tick/shutdown when not initialised: safe no-op
//
// Invariants:
//   - GDK task queue dispatched exactly once per frame on main thread
//   - After shutdown, xboxAvailable() returns false
//   - Suspend callback acknowledges within PLM timeout (~1 second)
//   - No GDK SDK types in public API
//
// Integration Points:
//   - Engine main loop: tickXbox() once per frame
//   - Engine init/shutdown: initXbox() / shutdownXbox()
//   - EventBus: suspend/resume/user-change events emitted

#include "xbox-config.h"
#include "xbox-context.h"
#include "xbox-types.h"

#include <optional>
#include <string_view>

namespace eng {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise the GDK runtime. Must be called early in engine startup,
/// before the first frame. Returns nullopt if GDK init fails.
/// Engine continues in non-Xbox mode (PC fallback).
/// Main thread only.
std::optional<XboxContext> initXbox(const XboxConfig& config);

/// Dispatch all pending GDK async callbacks. Call exactly once per frame
/// from the main loop. No-op if Xbox is not initialised.
/// Main thread only.
void tickXbox(const XboxContext& ctx);

/// Shut down the GDK runtime. Flushes pending saves and unregisters
/// callbacks. Safe to call on an uninitialised or already-shut-down context.
/// Main thread only.
void shutdownXbox(XboxContext& ctx);

/// Returns true if the GDK runtime was successfully initialised.
/// Thread-safe (reads an atomic flag).
bool xboxAvailable();

/// Returns the local user's Xbox user ID. Valid only when xboxAvailable()
/// and a user is signed in.
/// Main thread only.
XboxUserId xboxLocalUserId(const XboxContext& ctx);

/// Returns the local user's gamertag. Valid only when xboxAvailable()
/// and a user is signed in. The returned view is valid until shutdownXbox().
/// Main thread only.
std::string_view xboxLocalUserName(const XboxContext& ctx);

/// Returns the current lifecycle state (running, suspending, suspended,
/// resuming). Used by other subsystems to pause/resume operations.
/// Main thread only.
XboxLifecycleState xboxLifecycleState(const XboxContext& ctx);

}  // namespace eng
