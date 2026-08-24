#pragma once

#include <cstdint>

namespace eng {

struct Ps5UserId {
  /// PS5 system user ID (SceUserServiceUserId).
  int32_t value = -1;

  bool operator==(Ps5UserId other) const { return value == other.value; }
  bool operator!=(Ps5UserId other) const { return value != other.value; }
};

inline constexpr Ps5UserId PS5_USER_ID_INVALID{-1};

}  // namespace eng
