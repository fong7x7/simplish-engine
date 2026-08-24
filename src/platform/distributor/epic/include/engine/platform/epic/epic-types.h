#pragma once

// Design Summary -- Epic Platform Types
// Technical Approach: docs/technical-approaches/engine/platform-epic.md
//
// Behaviours:
//   - Define strong ID types for Epic product users, lobbies, connections
//   - Define EpicError enum for all Epic subsystem error reporting
//   - Define EpicP2PReliability, EpicLobbyPermission, EpicConnectionStatus
//   - Define EpicNotificationPosition, EpicPresenceStatus, EpicFriendStatus
//   - Define EpicLeaderboardEntry, EpicSessionInfo structs
//   - Provide constants for stat flush interval, max lobby members, cloud
//   limits
//
// Edge Cases:
//   - Invalid ID sentinel values for all strong ID types
//   - EpicError covers all subsystem failure modes in a single enum
//
// Invariants:
//   - No EOS SDK types appear in this header
//   - All enums use uint8_t backing for compact storage
//   - Strong ID types prevent accidental mixing of user/lobby/connection IDs
//
// Integration Points:
//   - All Epic subsystem headers depend on this types header

#include "epic-connection-handle.h"
#include "epic-leaderboard-entry.h"
#include "epic-lobby-id.h"
#include "epic-product-user-id.h"
#include "epic-session-info.h"

#include <cstdint>

namespace eng {

// ---------------------------------------------------------------------------
// Error enum
// ---------------------------------------------------------------------------

enum class EpicError : uint8_t {
  SUCCESS,
  NOT_AVAILABLE,
  INIT_FAILED,
  AUTH_FAILED,
  NOT_AUTHENTICATED,
  INVALID_ARGUMENT,

  // Cloud errors
  CLOUD_FILE_NOT_FOUND,
  CLOUD_WRITE_FAILED,
  CLOUD_READ_FAILED,
  CLOUD_QUOTA_EXCEEDED,

  // Networking errors
  CONNECTION_FAILED,
  SEND_FAILED,

  // Matchmaking errors
  LOBBY_CREATE_FAILED,
  LOBBY_JOIN_FAILED,
  LOBBY_FULL,
  NOT_LOBBY_OWNER,

  // Commerce errors
  ENTITLEMENT_QUERY_FAILED,
  REDEEM_FAILED,
};

// ---------------------------------------------------------------------------
// P2P reliability mode
// ---------------------------------------------------------------------------

enum class EpicP2PReliability : uint8_t {
  RELIABLE_ORDERED,
  UNRELIABLE,
};

// ---------------------------------------------------------------------------
// Lobby permission level
// ---------------------------------------------------------------------------

enum class EpicLobbyPermission : uint8_t {
  PUBLIC,
  FRIENDS_ONLY,
  INVITE_ONLY,
};

// ---------------------------------------------------------------------------
// Connection status
// ---------------------------------------------------------------------------

enum class EpicConnectionStatus : uint8_t {
  CONNECTING,
  CONNECTED,
  CLOSED,
};

// ---------------------------------------------------------------------------
// Notification position
// ---------------------------------------------------------------------------

enum class EpicNotificationPosition : uint8_t {
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_RIGHT,
};

// ---------------------------------------------------------------------------
// Presence status
// ---------------------------------------------------------------------------

enum class EpicPresenceStatus : uint8_t {
  ONLINE,
  AWAY,
  DO_NOT_DISTURB,
  OFFLINE,
};

// ---------------------------------------------------------------------------
// Friend status
// ---------------------------------------------------------------------------

enum class EpicFriendStatus : uint8_t {
  FRIENDS,
  INVITE_SENT,
  INVITE_RECEIVED,
  NOT_FRIENDS,
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline constexpr float EPIC_STATS_FLUSH_INTERVAL_S = 60.0f;
inline constexpr uint32_t EPIC_MAX_LOBBY_MEMBERS = 8;
inline constexpr uint32_t EPIC_CLOUD_MAX_FILE_SIZE_BYTES =
    200 * 1024 * 1024;  // 200 MB
inline constexpr uint32_t EPIC_CLOUD_MAX_TOTAL_BYTES =
    400 * 1024 * 1024;  // 400 MB

}  // namespace eng
