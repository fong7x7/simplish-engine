#pragma once

/// @file net-roster.h
/// @brief Which seats have a player connected.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::net {

/// Server to every seated client, whenever a seat fills or empties — what
/// a lobby shows.
struct NetRoster {
  /// A bit per input slot, set while a client is connected in it.
  uint8_t seated = 0;

  /// Rosters are equal when their masks are.
  bool operator==(const NetRoster&) const = default;
};

}  // namespace eng::net
