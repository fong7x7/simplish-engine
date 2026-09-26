#pragma once

/// @file net-input.h
/// @brief One client's input for one tick, on its way to the server.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/player-input.h>

namespace eng::net {

/// Client to server: the sender's input for `tick` of run `run`. Its seat
/// is the connection it arrives on.
struct NetInput {
  /// The run it is for.
  uint16_t run = 0;
  /// The tick it is for.
  uint64_t tick = 0;
  /// The input.
  sim::PlayerInput input;

  /// Inputs are equal when every field is.
  bool operator==(const NetInput&) const = default;
};

}  // namespace eng::net
