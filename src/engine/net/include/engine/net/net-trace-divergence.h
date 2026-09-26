#pragma once

/// @file net-trace-divergence.h
/// @brief The first tick and section on which peers' traces disagree.
/// @par Threading
/// Pure functions over value types.

#include <cstdint>
#include <engine/net/net-peer-trace.h>
#include <optional>
#include <span>
#include <string>

namespace eng::net {

/// Where a desynced run first diverged, found from its peers' traces.
struct NetTraceDivergence {
  /// The first tick two peers hashed differently.
  uint64_t tick = 0;
  /// The first section they differ on, by index, or
  /// `NET_SECTION_COUNT_DIFFERS`.
  uint8_t section = 0;
  /// That section's name, from the traces' names; empty when none has it.
  std::string section_name;
  /// The peer whose hash of that tick came first in the traces' order.
  uint8_t first_slot = 0;
  /// A peer that disagrees with it.
  uint8_t other_slot = 0;
};

/// The earliest tick on which any two of @p traces that both hashed it
/// disagree, and the first section they disagree on there. Nothing when
/// every tick the traces share agrees — the divergence is older than the
/// traces reach, or they do not overlap.
[[nodiscard]] std::optional<NetTraceDivergence>
findTraceDivergence(std::span<const NetPeerTrace> traces);

}  // namespace eng::net
