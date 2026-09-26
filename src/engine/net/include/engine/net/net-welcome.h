#pragma once

/// @file net-welcome.h
/// @brief A server giving a client its seat.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::net {

/// Server to client, in answer to a `NetHello`: the input slot the client
/// plays in, from now until it disconnects.
struct NetWelcome {
  /// The seat: an input slot, below `sim::MAX_PLAYERS`.
  uint8_t slot = 0;

  /// Welcomes are equal when their slots are.
  bool operator==(const NetWelcome&) const = default;
};

}  // namespace eng::net
