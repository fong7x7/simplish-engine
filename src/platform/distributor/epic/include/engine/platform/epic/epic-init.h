#pragma once

// Design Summary -- Epic Init & Lifecycle
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/init-lifecycle.md
//
// Behaviours:
//   - Initialise EOS SDK via EOS_Initialize() + EOS_Platform_Create()
//   - Authenticate via exchange code, persistent auth, or Device ID fallback
//   - Return EpicContext or nullopt on failure
//   - Dispatch pending EOS callbacks once per frame via tickEpicCallbacks()
//   - Shut down SDK and flush pending data via shutdownEpic()
//   - Query Epic availability, local user ID, display name, language
//
// Edge Cases:
//   - EOS SDK init fails: engine continues without Epic
//   - All auth methods fail: epicAvailable() returns false
//   - Tick/shutdown called when not initialised: safe no-op
//   - Auth token expires mid-session: automatic re-auth via callback
//   - Double init: second call returns nullopt with warning
//
// Invariants:
//   - EOS_Platform_Tick() called exactly once per frame on main thread
//   - After shutdown, epicAvailable() returns false
//   - No EOS SDK types in public API
//
// Integration Points:
//   - Engine main loop: tickEpicCallbacks() once per frame
//   - Engine init/shutdown: initEpic() / shutdownEpic()
//   - EventBus: EpicContext holds non-owning EventBus* for event emission

#include "epic-config.h"
#include "epic-context.h"
#include "epic-types.h"

#include <optional>
#include <string_view>

namespace eng {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise the EOS SDK. Must be called early in engine startup,
/// before the first frame. Returns nullopt if EOS SDK init fails,
/// credentials are invalid, or all auth methods fail. Engine
/// continues in non-Epic mode.
/// Main thread only.
std::optional<EpicContext> initEpic(const EpicConfig& config);

/// Dispatch all pending EOS callbacks. Call exactly once per frame
/// from the main loop. No-op if Epic is not initialised.
/// Main thread only.
void tickEpicCallbacks(const EpicContext& ctx);

/// Shut down the EOS SDK. Flushes pending cloud writes and stats.
/// Safe to call on an uninitialised or already-shut-down context.
/// Main thread only.
void shutdownEpic(EpicContext& ctx);

/// Returns true if EOS was successfully initialised and authenticated.
/// Thread-safe (reads an atomic flag).
bool epicAvailable();

/// Returns the local user's EOS Product User ID. Valid only when
/// epicAvailable(). Main thread only.
EpicProductUserId epicLocalUserId(const EpicContext& ctx);

/// Returns the local user's display name. Valid only when
/// epicAvailable(). The returned view is valid until shutdownEpic().
/// Main thread only.
std::string_view epicLocalUserName(const EpicContext& ctx);

/// Returns the EOS locale language code (e.g. "en", "fr").
/// Valid only when epicAvailable(). Main thread only.
std::string_view epicLanguage(const EpicContext& ctx);

}  // namespace eng
