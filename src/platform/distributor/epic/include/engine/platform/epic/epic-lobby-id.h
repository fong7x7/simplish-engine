#pragma once

#include <cstdint>

namespace eng {

struct EpicLobbyId {
  /// Opaque lobby identifier assigned by EOS matchmaking.
  uint64_t value = 0;

  bool operator==(EpicLobbyId other) const { return value == other.value; }
  bool operator!=(EpicLobbyId other) const { return value != other.value; }
};

inline constexpr EpicLobbyId EPIC_LOBBY_ID_INVALID{0};

}  // namespace eng
