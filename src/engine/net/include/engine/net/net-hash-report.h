#pragma once

/// @file net-hash-report.h
/// @brief A peer's state hash at a tick, for the server to compare.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/tick-hash.h>

namespace eng::net {

/// Ticks between hash reports: every tick divisible by this is reported,
/// once a second at 60 Hz.
inline constexpr uint64_t NET_HASH_INTERVAL = 60;

/// Client to server: the sender's hash at the end of `hash.tick`. Section
/// names do not travel; the receiver names sections from its own run.
struct NetHashReport {
  /// The run it is for.
  uint16_t run = 0;
  /// The hash, with its tick.
  sim::TickHash hash;
};

}  // namespace eng::net
