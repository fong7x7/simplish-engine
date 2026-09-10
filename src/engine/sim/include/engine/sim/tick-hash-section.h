#pragma once

/// @file tick-hash-section.h
/// @brief One subsystem's share of a tick hash.
/// @par Threading
/// A value type.

#include <cstdint>
#include <string_view>

namespace eng::sim {

/// The hash of one subsystem's state at the end of a tick. Sections are what
/// let a divergence name the subsystem that diverged, rather than only the
/// tick it diverged on (Engine REQUIREMENTS §4.3).
struct TickHashSection {
  /// The subsystem this covers. A string literal, so it outlives any hash
  /// that names it. Empty for a hash read back from a replay, which records
  /// section hashes but not their names.
  std::string_view name;
  /// The subsystem's state hash.
  uint64_t hash = 0;
};

}  // namespace eng::sim
