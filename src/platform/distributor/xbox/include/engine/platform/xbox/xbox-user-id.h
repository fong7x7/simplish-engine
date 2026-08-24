#pragma once

#include <cstdint>

namespace eng {

struct XboxUserId {
  /// Xbox Live user identifier (XUID).
  uint64_t value = 0;

  bool operator==(XboxUserId other) const { return value == other.value; }
  bool operator!=(XboxUserId other) const { return value != other.value; }
};

inline constexpr XboxUserId XBOX_USER_ID_INVALID{0};

}  // namespace eng
