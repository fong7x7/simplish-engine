#pragma once

/// @file net-waiting.h
/// @brief Who a stalled session is waiting on.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::net {

/// Server to every client in a run: the frame for `tick` is held up by the
/// seats in `waiting`, whose input for it has not come — what a client
/// shows while it stalls (ADR-005: "visibly, with feedback"). Whoever runs
/// the server decides when a wait has lasted long enough to say so; the
/// engine reads no clock.
struct NetWaiting {
  /// The run it is for.
  uint16_t run = 0;
  /// The tick whose frame is held up.
  uint64_t tick = 0;
  /// A bit per seat whose input has not come.
  uint8_t waiting = 0;

  /// Waits are equal when every field is.
  bool operator==(const NetWaiting&) const = default;
};

}  // namespace eng::net
