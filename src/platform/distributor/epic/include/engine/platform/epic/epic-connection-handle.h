#pragma once

#include <cstdint>

namespace eng {

struct EpicConnectionHandle {
  /// Opaque connection identifier assigned by the EOS SDK.
  uint32_t value = 0;

  bool operator==(EpicConnectionHandle other) const {
    return value == other.value;
  }
  bool operator!=(EpicConnectionHandle other) const {
    return value != other.value;
  }
};

inline constexpr EpicConnectionHandle EPIC_CONNECTION_INVALID{0};

}  // namespace eng
