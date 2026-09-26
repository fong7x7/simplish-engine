#pragma once

/// @file net-trace.h
/// @brief A peer's hash of every recent tick, sent when a run desyncs.
/// @par Threading
/// A value type.

#include <cstddef>
#include <cstdint>
#include <engine/sim/tick-hash.h>
#include <string>
#include <vector>

namespace eng::net {

/// Ticks of hashes a peer keeps, and sends in its trace: four seconds.
/// Hashes are compared every `NET_HASH_INTERVAL` ticks and peers run a few
/// ticks apart, so this reaches back past the last checkpoint every peer
/// agreed on — the first diverging tick is always in it.
inline constexpr std::size_t NET_TRACE_TICKS = 240;

/// Client to server, once told of a desync: the sender's hash of each of
/// its last ticks, every section, so the server can find the exact tick
/// and section that first diverged rather than the checkpoint that caught
/// it (ADR-005: "captures both peers' recent tick traces").
struct NetTrace {
  /// The run it is for.
  uint16_t run = 0;
  /// The sender's names for its sections, in order.
  std::vector<std::string> section_names;
  /// The hashes, oldest first, one per tick, without names. At most
  /// `NET_TRACE_TICKS`.
  std::vector<sim::TickHash> ticks;
};

}  // namespace eng::net
