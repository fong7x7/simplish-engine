#pragma once

/// @file behavior-facing.h
/// @brief What an actor turns to face while in one state.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// What a state has an actor turn toward, at its behavior's turn rate.
enum class BehaviorFacing : uint8_t {
  /// The way it is moving; when standing, a target it can see.
  MOVEMENT,
  /// Where its target was seen, whichever way it is moving — to strafe, to
  /// back away watching, to telegraph a charge.
  TARGET,
  /// Nothing: it keeps the heading it entered the state with, which is
  /// what makes a charge overshoot.
  LOCKED,
};

}  // namespace eng::game
