#pragma once

/// @file footstep-walker.h
/// @brief One character whose steps might be heard, as a tick left it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/content/step-set.h>

namespace eng::game {

/// Somebody walking, as `FootstepTracker` is told about them each tick.
struct FootstepWalker {
  /// Who they are, the same number every tick for as long as they walk: a
  /// player's input slot, or an actor's place in the setup past them.
  uint32_t key = 0;
  /// Where their feet are, in world tiles.
  Vec3 at{};
  /// What their feet sound like.
  StepSet steps = StepSet::DEFAULT;
};

}  // namespace eng::game
