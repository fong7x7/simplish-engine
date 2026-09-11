#pragma once

/// @file cylinder-collision.h
/// @brief Keeping a character out of solid geometry.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/physics/collision-box.h>
#include <engine/physics/collision-cylinder.h>
#include <span>

namespace eng::physics {

/// How far inside a box a character may be before it counts as overlapping,
/// in tiles. A character pushed out ends exactly touching; without this
/// margin the rounding of that push would read as a hair of overlap next
/// tick, and it would be pushed again.
inline constexpr float COLLISION_SKIN = 1.0e-4F;

/// Most passes a resolution makes over the boxes. One is enough for a
/// single box; a character wedged into a corner between two needs a second
/// to settle, and a third is margin.
inline constexpr int COLLISION_RESOLVE_PASSES = 3;

/// Whether @p cylinder is inside @p box by more than `COLLISION_SKIN`.
///
/// Only where their heights overlap: a box entirely above the character's
/// head or below its feet — a sign overhead, a decal on the floor — is not
/// in its way.
[[nodiscard]] bool cylinderOverlapsBox(const CollisionCylinder& cylinder,
                                       const CollisionBox& box);

/// Where @p cylinder's centre has to be for it to overlap none of
/// @p boxes: moved the shortest way out of each box it is inside.
///
/// A character that walks into a wall is pushed back to touch it, and one
/// that walks into it at an angle slides along it, which is what moving and
/// then resolving gives for free. One whose centre is inside a box — spawned
/// there, say — leaves by the nearest face.
///
/// Deterministic: the boxes are visited in the order given, a fixed number
/// of passes at most, with IEEE arithmetic and `std::sqrt`, which the
/// standard requires to be correctly rounded. Two machines resolving the
/// same cylinder against the same list agree to the bit.
///
/// Every box is tested: fine for a level's props and a handful of players.
/// A horde goes through the overload below, with a `BoxBroadphase`'s
/// candidates.
[[nodiscard]] Vec2
resolveCylinderAgainstBoxes(const CollisionCylinder& cylinder,
                            std::span<const CollisionBox> boxes);

/// The same, testing only the boxes at @p candidates — indices into
/// @p boxes, ascending, as `BoxBroadphase::gather` gives them. Visiting
/// the candidates in list order, it gives exactly what the full scan
/// gives whenever the candidates hold every box the cylinder comes near
/// while being pushed, which a gather reaching a tile past its radius
/// does for any push shorter than that.
[[nodiscard]] Vec2
resolveCylinderAgainstBoxes(const CollisionCylinder& cylinder,
                            std::span<const CollisionBox> boxes,
                            std::span<const uint32_t> candidates);

}  // namespace eng::physics
