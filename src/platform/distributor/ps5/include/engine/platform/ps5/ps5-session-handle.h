#pragma once

#include <cstdint>

namespace eng {

struct Ps5SessionHandle {
  /// Opaque PSN game session identifier.
  uint32_t value = 0;

  bool operator==(Ps5SessionHandle other) const { return value == other.value; }
  bool operator!=(Ps5SessionHandle other) const { return value != other.value; }
};

inline constexpr Ps5SessionHandle PS5_SESSION_INVALID{0};

}  // namespace eng
