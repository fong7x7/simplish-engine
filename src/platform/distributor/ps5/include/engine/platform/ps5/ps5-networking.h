#pragma once

// Design Summary -- PSN Networking
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/psn-networking.md
//
// Behaviours:
//   - Sign in to PSN via SceNpManager for online play
//   - Manage PSN sessions via SceGameIntentManager
//   - Send/accept friend invites via SceNpInvitation
//   - Update rich presence (world name, player count) via SceNpPresence
//   - Check parental controls before enabling online features
//   - ENet over libSceNet BSD socket shim for game data transport
//   - Graceful offline fallback when PSN is unavailable (TRC requirement)
//   - Respect PS5 network reachability events
//
// Edge Cases:
//   - PSN unavailable: online disabled, single-player works
//   - PSN sign-in fails: log warning, disable multiplayer
//   - Network lost mid-session: emit disconnect event, go offline
//   - Parental controls block online: disable multiplayer options
//   - DNS failure: bounded timeout, no hang (TRC requirement)
//   - PSN session invalidated on resume: re-validate or fall back
//
// Invariants:
//   - PSN SDK headers never in public header
//   - Single-player has zero PSN dependency
//   - ENet is data transport; PSN is session/identity only
//   - Main thread only
//   - Network state machine respects reachability events
//
// Integration Points:
//   - Engine Networking: PSN session first, then ENet transport
//   - EventBus: network state changes, invites, presence
//   - Suspend/resume: PSN session re-validated on resume

#include "ps5-networking-config.h"
#include "ps5-networking-context.h"
#include "ps5-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <optional>
#include <string_view>

namespace eng {

// Forward declarations
struct Ps5PlatformContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise PS5 networking (libSceNet, SceNpManager). Returns nullopt
/// if network subsystem initialisation fails.
/// Main thread only.
std::optional<Ps5NetworkingContext>
initPs5Networking(const Ps5NetworkingConfig& config);

/// Shut down PS5 networking. Destroys active sessions and releases
/// network resources. Safe to call when not initialised.
/// Main thread only.
void shutdownPs5Networking(Ps5NetworkingContext& ctx);

/// Tick PSN events. Polls network reachability, processes pending
/// PSN callbacks. Call once per frame.
/// Main thread only.
void tickPs5Networking(Ps5NetworkingContext& ctx);

/// Sign in to PSN. Required before any online play.
/// Returns error if sign-in fails or PSN is unavailable.
/// Main thread only.
std::expected<void, Ps5Error> psnSignIn(Ps5NetworkingContext& ctx);

/// Returns true if PSN is signed in and network is available.
bool psnAvailable(const Ps5NetworkingContext& ctx);

/// Returns the current PSN network status.
Ps5NetworkStatus psnNetworkStatus(const Ps5NetworkingContext& ctx);

/// Create a PSN game session. Registers activity on PS5 UI.
/// Main thread only.
std::expected<Ps5SessionHandle, Ps5Error>
createSession(Ps5NetworkingContext& ctx);

/// Destroy a PSN game session.
/// Main thread only.
void destroySession(Ps5NetworkingContext& ctx, Ps5SessionHandle session);

/// Send a friend invite to a PSN user. Invite appears in PS5 overlay.
/// Main thread only.
std::expected<void, Ps5Error> sendInvite(const Ps5NetworkingContext& ctx,
                                         Ps5UserId target);

/// Update rich presence shown on friend lists.
/// Main thread only.
void updatePresence(const Ps5NetworkingContext& ctx,
                    std::string_view world_name, uint32_t player_count);

/// Check if parental controls allow online play for the signed-in user.
bool parentalControlAllowsOnline(const Ps5NetworkingContext& ctx);

}  // namespace eng
