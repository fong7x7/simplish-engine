#pragma once

#include <cstdint>

namespace eng {

struct SteamInputHandle {
  /// Opaque handle identifying a Steam Input controller.
  uint64_t value = 0;

  bool operator==(SteamInputHandle other) const { return value == other.value; }
  bool operator!=(SteamInputHandle other) const { return value != other.value; }
};

inline constexpr SteamInputHandle STEAM_INPUT_HANDLE_INVALID{0};

}  // namespace eng
