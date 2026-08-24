#pragma once

// Design Summary -- Steam Matchmaking
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/matchmaking.md
//
// Behaviours:
//   - Create a Steam lobby (private, friends-only, public) with max member
//   count
//   - Join an existing lobby by ID
//   - Invite a Steam friend to a lobby
//   - Set lobby metadata (world name, player count, game version)
//   - Set lobby game server address for peer connection
//   - Leave a lobby
//
// Edge Cases:
//   - Steam not available: all operations return NOT_AVAILABLE
//   - Lobby creation failure: returns LOBBY_CREATE_FAILED
//   - Join non-existent lobby: returns LOBBY_JOIN_FAILED
//   - Join full lobby: returns LOBBY_FULL
//   - Set metadata without ownership: returns NOT_LOBBY_OWNER
//   - Invite offline friend: Steam handles silently
//
// Invariants:
//   - Only lobby owner can set metadata and game server
//   - Lobby events emitted via EventBus; game code does not poll
//   - SteamLobbyId is a strong typedef; no raw uint64_t in public API
//   - Main thread only
//
// Integration Points:
//   - Engine Networking: game server address used for peer connections
//   - EventBus: lobby events for game UI

#include "steam-lobby-game-server-params.h"
#include "steam-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <string_view>

namespace eng {

// Forward declaration
struct SteamContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Create a Steam lobby. The lobby ID is delivered asynchronously via
/// a SteamLobbyCreated EventBus event. Returns a pending lobby ID on
/// success or an error if lobby creation could not be initiated.
/// Main thread only.
std::expected<SteamLobbyId, SteamError>
createLobby(const SteamContext& ctx, SteamLobbyType type, uint32_t max_members);

/// Join an existing lobby. Completion is delivered via a
/// SteamLobbyJoined EventBus event.
/// Main thread only.
std::expected<bool, SteamError> joinLobby(const SteamContext& ctx,
                                          SteamLobbyId lobby_id);

/// Leave a lobby. Safe to call even if not in a lobby.
/// Main thread only.
void leaveLobby(const SteamContext& ctx, SteamLobbyId lobby_id);

/// Invite a Steam friend to the specified lobby.
/// Main thread only.
std::expected<bool, SteamError> inviteToLobby(const SteamContext& ctx,
                                              SteamLobbyId lobby_id,
                                              SteamUserId friend_id);

/// Set a key-value pair in the lobby's metadata. Only the lobby owner
/// may set metadata. Common keys: "world_name", "player_count",
/// "game_version".
/// Main thread only.
std::expected<bool, SteamError> setLobbyData(const SteamContext& ctx,
                                             SteamLobbyId lobby_id,
                                             std::string_view key,
                                             std::string_view value);

/// Advertise the game server's address to lobby members. Peers read
/// this to initiate connections via the negotiated transport.
/// Main thread only.
std::expected<bool, SteamError>
setLobbyGameServer(const SteamContext& ctx, SteamLobbyId lobby_id,
                   const SteamLobbyGameServerParams& server);

}  // namespace eng
