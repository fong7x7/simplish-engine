#pragma once

/// @file collision-cylinder.h
/// @brief The upright cylinder a character collides as.
/// @par Threading
/// A value type.

#include <engine/math/vec2.h>

namespace eng::physics {

/// A character, as collision sees one: a circle on the ground stood up to a
/// height. The capsule Game §3.1 asks for, less the rounded ends — which
/// only matter once there are slopes and steps to ride over.
struct CollisionCylinder {
  /// Where the circle's centre is, in world X and Y.
  Vec2 center{};
  /// The circle's radius, in tiles.
  float radius = 0.0F;
  /// World Z of the character's feet.
  float bottom = 0.0F;
  /// How tall the character stands, in tiles.
  float height = 0.0F;
};

}  // namespace eng::physics
