#pragma once

#include <cstdint>

namespace eng {

struct SteamConnectionHandle {
  /// Opaque connection identifier assigned by Steam Networking Sockets.
  uint32_t value = 0;

  bool operator==(SteamConnectionHandle other) const {
    return value == other.value;
  }
  bool operator!=(SteamConnectionHandle other) const {
    return value != other.value;
  }
};

inline constexpr SteamConnectionHandle STEAM_CONNECTION_INVALID{0};

}  // namespace eng
