#pragma once

// Design Summary -- Steam Platform Types
// Technical Approach: docs/technical-approaches/engine/platform-steam.md
//
// Behaviours:
//   - Define strong ID types for Steam users, lobbies, connections, listeners,
//   input handles
//   - Define SteamError enum for all Steam subsystem error reporting
//   - Define SteamTransportMode, SteamLobbyType, SteamSendFlags,
//   SteamConnectionStatus enums
//   - Define SteamNotificationPosition, SteamFloatingKeyboardMode enums
//   - Define SteamTriggerEffectMode and SteamTriggerEffectParams for DualSense
//   haptics
//   - Provide constants for stat flush interval, max lobby members, cloud path
//   limit
//
// Edge Cases:
//   - Invalid ID sentinel values for all strong ID types
//   - SteamError covers all subsystem failure modes in a single enum
//
// Invariants:
//   - No Steamworks SDK types appear in this header
//   - All enums use uint8_t backing for compact storage
//   - Strong ID types prevent accidental mixing of user/lobby/connection IDs
//
// Integration Points:
//   - All Steam subsystem headers depend on this types header

#include "steam-connection-handle.h"
#include "steam-input-handle.h"
#include "steam-listener-handle.h"
#include "steam-lobby-id.h"
#include "steam-trigger-effect-params.h"
#include "steam-user-id.h"

#include <cstdint>

namespace eng {

// ---------------------------------------------------------------------------
// Error enum
// ---------------------------------------------------------------------------

enum class SteamError : uint8_t {
  NOT_AVAILABLE,
  INIT_FAILED,
  RESTART_REQUIRED,
  INVALID_ARGUMENT,

  // Cloud errors
  CLOUD_DISABLED,
  CLOUD_FILE_NOT_FOUND,
  CLOUD_WRITE_FAILED,
  CLOUD_READ_FAILED,

  // Input errors
  ACTION_SET_NOT_FOUND,

  // Networking errors
  CONNECTION_FAILED,
  SEND_FAILED,
  LISTENER_FAILED,

  // Matchmaking errors
  LOBBY_CREATE_FAILED,
  LOBBY_JOIN_FAILED,
  LOBBY_FULL,
  NOT_LOBBY_OWNER,

  // Workshop errors
  WORKSHOP_ITEM_NOT_FOUND,
  WORKSHOP_ITEM_NOT_INSTALLED,
  WORKSHOP_PUBLISH_FAILED,
};

// ---------------------------------------------------------------------------
// Transport mode
// ---------------------------------------------------------------------------

enum class SteamTransportMode : uint8_t {
  RELAY,
  DIRECT,
};

// ---------------------------------------------------------------------------
// Lobby type
// ---------------------------------------------------------------------------

enum class SteamLobbyType : uint8_t {
  PRIVATE,
  FRIENDS_ONLY,
  PUBLIC,
};

// ---------------------------------------------------------------------------
// Send flags for networking
// ---------------------------------------------------------------------------

enum class SteamSendFlags : uint8_t {
  UNRELIABLE,
  RELIABLE,
  NO_NAGLE,
};

// ---------------------------------------------------------------------------
// Connection status
// ---------------------------------------------------------------------------

enum class SteamConnectionStatus : uint8_t {
  CONNECTING,
  CONNECTED,
  CLOSED,
  PROBLEM_DETECTED,
};

// ---------------------------------------------------------------------------
// Overlay notification position
// ---------------------------------------------------------------------------

enum class SteamNotificationPosition : uint8_t {
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_RIGHT,
};

// ---------------------------------------------------------------------------
// Floating keyboard mode (Steam Deck)
// ---------------------------------------------------------------------------

enum class SteamFloatingKeyboardMode : uint8_t {
  SINGLE_LINE,
  MULTI_LINE,
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline constexpr float STEAM_STATS_FLUSH_INTERVAL_S = 60.0f;
inline constexpr uint32_t STEAM_MAX_LOBBY_MEMBERS = 8;
inline constexpr uint32_t STEAM_CLOUD_PATH_MAX_BYTES = 256;
inline constexpr uint32_t STEAM_MAX_CONTROLLER_HANDLES = 16;

}  // namespace eng
