#pragma once

// Design Summary -- Xbox Series X Networking (Xbox Live)
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x/networking.md
//
// Behaviours:
//   - Sign in Xbox user via XUserAddAsync
//   - Create multiplayer session via MPSD (XblMultiplayerManager)
//   - Send game invites to friends via Xbox Guide overlay
//   - Set rich presence (world name, player count)
//   - Check user privileges before enabling online features
//   - Monitor network connectivity changes
//   - Disable online features gracefully when Xbox Live unavailable
//   - Handle Xbox Live service outages without crashes
//
// Edge Cases:
//   - Xbox Live unavailable: return LIVE_NOT_AVAILABLE, offline ok
//   - User lacks Gold/Game Pass Core: multiplayer disabled
//   - Parental controls block online: PRIVILEGE_DENIED
//   - Network lost mid-session: emit event, transition to offline
//   - MPSD session creation failure: return error, offer retry
//   - Invite received in wrong game state: queue, process when safe
//   - User signs out during multiplayer: emit event, return to title
//
// Invariants:
//   - ENet is actual UDP transport; Xbox Live manages sessions/social only
//   - Offline single-player has no Xbox Live dependency
//   - All XSAPI calls async; results via task queue
//   - Privilege checks per-action, not cached
//   - Network connectivity monitored continuously
//
// Integration Points:
//   - Engine networking: ENet provides transport, Xbox Live manages sessions
//   - EventBus: connectivity/invite events emitted as engine events
//   - UI: privilege denial messages shown through engine GUI

#include "xbox-networking-context.h"
#include "xbox-session-params.h"
#include "xbox-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <string_view>

namespace eng {

// Forward declarations
struct XboxContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise Xbox Live networking. Registers connectivity hint callback
/// and prepares XSAPI. Must be called after GDK runtime init.
/// Main thread only.
std::expected<XboxNetworkingContext, XboxError>
initXboxNetworking(const XboxContext& ctx, EventBus* event_bus);

/// Shut down Xbox Live networking. Destroys active sessions and
/// unregisters callbacks. Idempotent.
/// Main thread only.
void shutdownXboxNetworking(XboxNetworkingContext& ctx);

/// Sign in a local Xbox user. Triggers the system sign-in UI if needed.
/// Returns the signed-in user ID on success.
/// Main thread only.
std::expected<XboxUserId, XboxError> signInXboxUser(XboxNetworkingContext& ctx);

/// Create a multiplayer session via MPSD. Returns a session handle
/// for invite and presence operations.
/// Main thread only.
std::expected<XboxSessionHandle, XboxError>
createXboxSession(XboxNetworkingContext& ctx, const XboxSessionParams& params);

/// Destroy a multiplayer session. Removes the session from MPSD.
/// Idempotent.
/// Main thread only.
void destroyXboxSession(XboxNetworkingContext& ctx, XboxSessionHandle session);

/// Send a game invite to friends via the Xbox Guide overlay.
/// Opens the system invite UI for the given session.
/// Main thread only.
void sendXboxInvite(XboxNetworkingContext& ctx, XboxSessionHandle session);

/// Set rich presence string. Displayed on the friend list
/// (e.g. "Playing in World 'MyWorld' - 4/8 players").
/// Main thread only.
void setXboxPresence(XboxNetworkingContext& ctx,
                     std::string_view presence_string);

/// Check whether the signed-in user has a specific privilege.
/// Must be called before enabling any online feature.
/// Returns ALLOWED, DENIED, or NOT_SIGNED_IN.
/// Main thread only.
XboxPrivilegeResult checkXboxPrivilege(const XboxNetworkingContext& ctx,
                                       XboxPrivilege privilege);

/// Returns the current network connectivity state.
/// Updated continuously via connectivity hint callback.
/// Main thread only.
XboxNetworkState xboxNetworkState(const XboxNetworkingContext& ctx);

/// Process MPSD updates and connectivity changes. Call once per frame.
/// Main thread only.
void tickXboxNetworking(XboxNetworkingContext& ctx);

}  // namespace eng
