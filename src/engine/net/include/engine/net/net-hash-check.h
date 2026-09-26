#pragma once

/// @file net-hash-check.h
/// @brief The first hash reported for a tick, which later ones must match.
/// @par Threading
/// A value type.

#include <cstddef>
#include <cstdint>
#include <engine/sim/tick-hash.h>

namespace eng::net {

/// Hash checkpoints a server remembers: sixteen seconds of reports, far
/// longer than any two clients in a live session can be apart.
inline constexpr std::size_t NET_HASH_RING = 16;

/// A reported hash the server compares later reports of the same tick with.
struct NetHashCheck {
  /// The run it was reported in; checks from another run are stale.
  uint16_t run = 0;
  /// The first report of its tick.
  sim::TickHash hash;
  /// 1 once a report is here.
  uint8_t taken = 0;
};

}  // namespace eng::net
