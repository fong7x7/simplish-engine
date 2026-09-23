#pragma once

/// @file footstep-gait.h
/// @brief One walker's progress towards its next step.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec3.h>

namespace eng::game {

/// Where a walker was, and how far it has come since it last stepped: what
/// `FootstepTracker` keeps for each walker between ticks.
struct FootstepGait {
  /// Where it was last tick.
  Vec3 last{};
  /// How far it has come since its last step, in tiles.
  float travelled = 0.0F;
  /// The round of `FootstepTracker::advance` that last saw it.
  uint64_t seen = 0;
};

}  // namespace eng::game
