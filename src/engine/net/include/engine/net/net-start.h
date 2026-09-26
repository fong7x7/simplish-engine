#pragma once

/// @file net-start.h
/// @brief A server starting a run, at a level boundary.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/replay-header.h>

namespace eng::net {

/// Server to every seated client: build the simulation from this, at tick
/// 0, and play it. The header is the replay header of the run (ADR-005: a
/// recorded session is a replay), so every peer builds the same one.
struct NetStart {
  /// The run's number in this session. Every later message about the run
  /// carries it, so nothing meant for an earlier run is taken for this one.
  uint16_t run = 0;
  /// What the run starts from: level, content hash, seed, player count and
  /// each seat's character.
  sim::ReplayHeader header;
  /// Ticks between sampling an input and simulating it (ADR-005).
  uint8_t input_delay = 0;

  /// Starts are equal when every field is.
  bool operator==(const NetStart&) const = default;
};

}  // namespace eng::net
