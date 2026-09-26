#pragma once

/// @file net-peer-trace.h
/// @brief One peer's trace, and whose it is.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/net/net-trace.h>

namespace eng::net {

/// A trace a server holds after a desync: a seat's, or its own when it
/// simulates the run (`NET_SERVER_SLOT`).
struct NetPeerTrace {
  /// Whose: an input slot, or `NET_SERVER_SLOT`.
  uint8_t slot = 0;
  /// The trace.
  NetTrace trace;
};

}  // namespace eng::net
