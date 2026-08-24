#pragma once

#include <cstdint>

namespace eng {

struct EpicProductUserId {
  /// Opaque EOS product user identifier.
  uint64_t value = 0;

  bool operator==(EpicProductUserId other) const {
    return value == other.value;
  }
  bool operator!=(EpicProductUserId other) const {
    return value != other.value;
  }
};

inline constexpr EpicProductUserId EPIC_USER_ID_INVALID{0};

}  // namespace eng
