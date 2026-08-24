#pragma once

#include "steam-user-id.h"

#include <cstdint>
#include <string_view>

namespace eng {

/// Parameters for advertising a game server's address to lobby members.
struct SteamLobbyGameServerParams {
  /// IP address of the game server (e.g. "127.0.0.1").
  std::string_view ip{};
  /// Port the game server is listening on.
  uint16_t port{};
  /// Steam user ID of the game server host.
  SteamUserId server_id;
};

}  // namespace eng
