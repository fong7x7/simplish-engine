#pragma once

#include <cstdint>

namespace eng {

struct XboxSoundHandle {
  /// Opaque handle identifying a playing spatial audio object.
  uint32_t value = 0;

  bool operator==(XboxSoundHandle other) const { return value == other.value; }
  bool operator!=(XboxSoundHandle other) const { return value != other.value; }
};

inline constexpr XboxSoundHandle XBOX_SOUND_HANDLE_INVALID{0};

}  // namespace eng
