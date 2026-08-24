#pragma once

#include <cstdint>

namespace eng {

struct SteamListenerHandle {
  /// Opaque handle identifying a Steam Networking Sockets listener.
  uint32_t value = 0;

  bool operator==(SteamListenerHandle other) const {
    return value == other.value;
  }
  bool operator!=(SteamListenerHandle other) const {
    return value != other.value;
  }
};

inline constexpr SteamListenerHandle STEAM_LISTENER_INVALID{0};

}  // namespace eng
