#pragma once

/// @file hash-divergence.h
/// @brief Where two hashes of the same tick disagree.
/// @par Threading
/// Pure functions over value types.

#include <cstdint>
#include <engine/sim/tick-hash.h>
#include <optional>
#include <string_view>

namespace eng::sim {

/// A tick on which two runs disagree, and the first subsystem that does.
struct HashDivergence {
  /// The tick both hashes were taken at.
  uint64_t tick = 0;
  /// The first section whose hash differs, named from the live run. Empty
  /// when the two hashes disagree on how many sections there are — a sign
  /// the two runs were built from different code, not that one diverged.
  std::string_view section;
};

/// Compares a recorded or remote hash with a live one for the same tick.
/// Nothing when they agree. Section names are taken from `actual`, since a
/// hash decoded from a replay carries none.
std::optional<HashDivergence> findDivergence(const TickHash& expected,
                                             const TickHash& actual);

}  // namespace eng::sim
