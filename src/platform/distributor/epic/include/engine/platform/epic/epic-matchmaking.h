#pragma once

// Design Summary -- Epic Matchmaking
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/matchmaking.md
//
// Behaviours:
//   - Create EOS lobby with max players, permission, attributes
//   - Join lobby via invite or search
//   - Send friend invite to lobby
//   - Update lobby attributes (world name, player count, version)
//   - Leave lobby
//   - Search for dedicated server sessions
//
// Edge Cases:
//   - EOS not available: callbacks receive error
//   - Lobby full: LOBBY_FULL error
//   - Not lobby owner: NOT_LOBBY_OWNER error
//   - Session search with no results: empty vector
//
// Invariants:
//   - All matchmaking operations are async
//   - Lobby state synced via EOS callbacks on main thread
//   - No EOS SDK types in public API
//
// Integration Points:
//   - Networking: peers connect via negotiated transport after lobby
//   - Social: friends list provides invite targets
//   - EventBus: lobby join/invite events emitted

#include "epic-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <functional>
#include <string_view>
#include <vector>

namespace eng {

// Forward declarations
struct EpicContext;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

struct EpicLobbyConfig {
  /// Maximum number of players allowed in the lobby.
  uint32_t max_members = EPIC_MAX_LOBBY_MEMBERS;
  /// Visibility and access rules for the lobby.
  EpicLobbyPermission permission_level = EpicLobbyPermission::PUBLIC;
};

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

using EpicLobbyCreateCallback =
    std::function<void(std::expected<EpicLobbyId, EpicError> result)>;

using EpicLobbyJoinCallback =
    std::function<void(std::expected<EpicLobbyId, EpicError> result)>;

using EpicSessionSearchCallback = std::function<void(
    std::expected<std::vector<EpicSessionInfo>, EpicError> result)>;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Create a new EOS lobby. Async; callback receives lobby ID or
/// error. Main thread only.
void epicCreateLobby(const EpicContext& ctx, const EpicLobbyConfig& config,
                     const EpicLobbyCreateCallback& callback);

/// Join an existing lobby. Async; callback receives lobby ID or
/// error. Main thread only.
void epicJoinLobby(const EpicContext& ctx, EpicLobbyId lobby_id,
                   const EpicLobbyJoinCallback& callback);

/// Send a lobby invite to a friend. Fire-and-forget; failure logged.
/// Main thread only.
void epicSendLobbyInvite(const EpicContext& ctx, EpicLobbyId lobby_id,
                         EpicProductUserId target_user);

/// Set a lobby attribute (key-value pair). Only callable by the
/// lobby owner. Fire-and-forget; failure logged.
/// Main thread only.
void epicSetLobbyAttribute(const EpicContext& ctx, EpicLobbyId lobby_id,
                           std::string_view key, std::string_view value);

/// Leave a lobby. Fire-and-forget. Safe to call when not in a
/// lobby. Main thread only.
void epicLeaveLobby(const EpicContext& ctx, EpicLobbyId lobby_id);

/// Search for available game sessions. Async; callback receives
/// session info list or error. Main thread only.
void epicSearchSessions(const EpicContext& ctx, std::string_view search_param,
                        uint32_t max_results,
                        const EpicSessionSearchCallback& callback);

}  // namespace eng
