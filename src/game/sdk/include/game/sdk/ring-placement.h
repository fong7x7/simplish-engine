#pragma once

/// @file ring-placement.h
/// @brief Whether a spawn pattern checks the floor it puts actors on.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game::sdk {

/// Where a pattern may put an actor.
enum class RingPlacement : uint8_t {
  /// Only on walkable floor: a spot inside a prop is skipped.
  WALKABLE_ONLY,
  /// Wherever the pattern says.
  ANYWHERE,
};

}  // namespace eng::game::sdk
