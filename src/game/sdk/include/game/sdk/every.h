#pragma once

/// @file every.h
/// @brief Something that happens every so many ticks.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game::sdk {

/// A beat: due on tick `offset`, then every `interval` ticks after it.
/// Plain numbers of the tick, so it needs no hashing and cannot drift.
///
/// @code
///   constexpr sdk::Every WAVES{sdk::seconds(20)};
///   if (WAVES.due(world.tick())) { ... }
/// @endcode
struct Every {
  /// Ticks between beats; 0 is never due.
  uint64_t interval = 0;
  /// The first tick it is due on.
  uint64_t offset = 0;

  /// Whether it is due on @p tick.
  [[nodiscard]] constexpr bool due(uint64_t tick) const {
    return interval != 0 && tick >= offset && (tick - offset) % interval == 0;
  }
};

}  // namespace eng::game::sdk
