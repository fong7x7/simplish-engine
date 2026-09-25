#pragma once

/// @file cooldown.h
/// @brief Something that, once done, cannot be done again for a while.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game::sdk {

/// Ready until started, then not ready for as long as it was started for.
/// A member of the logic's: hash it — `hash.add(cooldown)` — since a later
/// tick decides by it.
struct Cooldown {
  /// The first tick it is ready again.
  uint64_t ready_at = 0;

  /// Whether it is ready on @p tick.
  [[nodiscard]] constexpr bool ready(uint64_t tick) const {
    return tick >= ready_at;
  }
  /// Start it on @p tick, for @p length ticks.
  constexpr void start(uint64_t tick, uint64_t length) {
    ready_at = tick + length;
  }
};

}  // namespace eng::game::sdk
