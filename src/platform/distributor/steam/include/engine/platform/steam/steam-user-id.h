#pragma once

#include <cstdint>

namespace eng {

struct SteamUserId {
  /// 64-bit Steam user identifier (CSteamID).
  uint64_t value = 0;

  bool operator==(SteamUserId other) const { return value == other.value; }
  bool operator!=(SteamUserId other) const { return value != other.value; }
};

inline constexpr SteamUserId STEAM_USER_ID_INVALID{0};

}  // namespace eng
