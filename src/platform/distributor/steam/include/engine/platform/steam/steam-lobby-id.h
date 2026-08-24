#pragma once

#include <cstdint>

namespace eng {

struct SteamLobbyId {
  /// Opaque lobby identifier assigned by Steam matchmaking.
  uint64_t value = 0;

  bool operator==(SteamLobbyId other) const { return value == other.value; }
  bool operator!=(SteamLobbyId other) const { return value != other.value; }
};

inline constexpr SteamLobbyId STEAM_LOBBY_ID_INVALID{0};

}  // namespace eng
