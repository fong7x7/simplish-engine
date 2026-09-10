#pragma once

/// @file collision-box.h
/// @brief A solid, axis-aligned box of level geometry.
/// @par Threading
/// A value type.

#include <engine/math/vec3.h>

namespace eng::physics {

/// A box nothing may stand inside: a prop, a wall, a crate. Axis-aligned in
/// world space, in tiles.
///
/// Static geometry is part of what a run starts from, like its content, so
/// a list of these is fixed for the run and every peer holds the same one
/// in the same order.
struct CollisionBox {
  /// The corner with the smallest X, Y and Z.
  Vec3 min{};
  /// The corner with the largest X, Y and Z.
  Vec3 max{};
};

}  // namespace eng::physics
