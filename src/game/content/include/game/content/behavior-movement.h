#pragma once

/// @file behavior-movement.h
/// @brief How fast an actor moves and turns.
/// @par Threading
/// A value type.

namespace eng::game {

/// An actor's movement.
struct BehaviorMovement {
  /// How fast it walks at full speed, in tiles a second — the unit the
  /// characters table uses.
  float speed = 3.5F;
  /// How fast it turns, in degrees a second.
  float turn_degrees_per_second = 360.0F;
};

}  // namespace eng::game
