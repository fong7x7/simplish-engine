#pragma once

/// @file tick-hash.h
/// @brief The hash of all simulation state at the end of one tick.
/// @par Threading
/// A value type.

#include <array>
#include <cstddef>
#include <cstdint>
#include <engine/sim/tick-hash-section.h>
#include <span>

namespace eng::sim {

/// Sections one tick hash can hold.
inline constexpr std::size_t MAX_TICK_HASH_SECTIONS = 16;

/// All simulation state at the end of `tick`, as one combined hash and as a
/// hash per subsystem. Two runs agree on a tick when their combined hashes
/// match; when they do not, the first differing section says where to look.
///
/// Fixed size, so taking one every tick allocates nothing.
struct TickHash {
  /// The tick this hash was taken at the end of.
  uint64_t tick = 0;
  /// Every section's hash, folded in section order.
  uint64_t combined = 0;
  /// The sections, in the order the game's `hashState` started them.
  std::array<TickHashSection, MAX_TICK_HASH_SECTIONS> sections{};
  /// Sections in use, from the front of `sections`.
  std::size_t section_count = 0;

  /// The sections in use.
  [[nodiscard]] std::span<const TickHashSection> activeSections() const {
    return {sections.data(), section_count};
  }
};

}  // namespace eng::sim
