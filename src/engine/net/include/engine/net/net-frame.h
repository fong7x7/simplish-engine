#pragma once

/// @file net-frame.h
/// @brief Every seat's input for one tick, confirmed by the server.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/tick-input.h>

namespace eng::net {

/// Server to every client in a run: `tick` may be simulated, on `input`.
/// A client simulates only these, in order (ADR-013).
///
/// A seat in `absent` has no player — never had one this run, or dropped —
/// and its input here is zero. Every peer fills it the same way from its
/// own copy of the world (the game's stand-in) before stepping, so the
/// dropped player's character plays on (ADR-005) without the server
/// knowing anything about the game.
struct NetFrame {
  /// The run it is for.
  uint16_t run = 0;
  /// The tick it confirms.
  uint64_t tick = 0;
  /// A bit per input slot with no player.
  uint8_t absent = 0;
  /// Every seat's input; seats at or past the run's player count are zero.
  sim::TickInput input;

  /// Frames are equal when every field is.
  bool operator==(const NetFrame&) const = default;
};

}  // namespace eng::net
