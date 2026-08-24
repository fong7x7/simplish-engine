#pragma once

#include <cstdint>

namespace eng {

struct XboxSessionHandle {
  /// Opaque MPSD multiplayer session identifier.
  uint32_t value = 0;

  bool operator==(XboxSessionHandle other) const {
    return value == other.value;
  }
  bool operator!=(XboxSessionHandle other) const {
    return value != other.value;
  }
};

inline constexpr XboxSessionHandle XBOX_SESSION_HANDLE_INVALID{0};

}  // namespace eng
