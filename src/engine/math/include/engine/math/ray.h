#pragma once

#include <engine/math/vec3.h>

namespace eng {

/// World-space ray: an origin point and a unit-length direction.
/// @thread_safety Immutable value type — safe to use from any thread.
struct Ray {
  /// World-space origin point.
  Vec3 origin;
  /// Unit-length world-space direction.
  Vec3 direction;
};

}  // namespace eng
