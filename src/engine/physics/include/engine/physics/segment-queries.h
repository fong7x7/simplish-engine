#pragma once

/// @file segment-queries.h
/// @brief Where a small moving sphere, stepping from one point to another,
/// first touches a box or an upright body.
/// @par Threading
/// Pure functions.

#include <engine/math/vec2.h>
#include <engine/physics/collision-box.h>
#include <optional>

namespace eng::physics {

/// A projectile's step: a sphere of `radius` tiles at height `z`, moving
/// across the floor from `from` to `to` in one tick.
struct SegmentSweep {
  /// Where the step starts, on the floor.
  Vec2 from{};
  /// Where it ends.
  Vec2 to{};
  /// The sphere's radius, in tiles.
  float radius = 0.0F;
  /// Its height above the floor, in tiles.
  float z = 0.0F;
};

/// How far along @p sweep, as a fraction from 0 to 1, the sphere first
/// touches @p box — nothing when it never does, or when the box is below
/// or above its height. A sphere that starts inside touches at 0.
///
/// The box is grown by the radius on every side and the step treated as a
/// segment (a slab test), so a sphere passing a corner diagonally is
/// stopped a hair early: conservative, and deterministic — IEEE
/// arithmetic only, the same on every machine.
[[nodiscard]] std::optional<float> sweepHitsBox(const SegmentSweep& sweep,
                                                const CollisionBox& box);

/// How far along @p sweep the sphere first touches an upright cylinder
/// round @p center of @p radius tiles, as a fraction from 0 to 1 — nothing
/// when it never does. Height is not tested: a body is as tall as anything
/// fired at it.
[[nodiscard]] std::optional<float> sweepHitsCircle(const SegmentSweep& sweep,
                                                   Vec2 center, float radius);

}  // namespace eng::physics
