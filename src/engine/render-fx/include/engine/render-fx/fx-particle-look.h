#pragma once

/// @file fx-particle-look.h
/// @brief How one particle looks and moves from birth to death.
/// @par Threading
/// A value type.

#include <engine/render-fx/fx-color.h>
#include <engine/render-fx/fx-particle-lighting.h>
#include <engine/render-fx/fx-particle-shape.h>

namespace eng {

/// Everything about a particle that its burst gives every particle alike:
/// size and colour at birth and at death, how it falls, how it slows, and
/// whether it streaks. What differs between a burst's particles — which
/// way, how fast, how long — is drawn when it is emitted.
struct FxParticleLook {
  /// Radius at birth, in tiles.
  float size_start = 0.1f;
  /// Radius when it dies, in tiles; it grows or shrinks linearly between.
  float size_end = 0.1f;
  /// Colour at birth.
  FxColor color_start{};
  /// Colour when it dies; it fades linearly between the two.
  FxColor color_end{};
  /// Downward acceleration, in tiles per second squared. Negative rises,
  /// which is what smoke does.
  float gravity = 0.0f;
  /// How quickly it slows: its speed falls by a factor of e every
  /// `1 / drag` seconds. Zero keeps its speed.
  float drag = 0.0f;
  /// Seconds of its own travel it is drawn stretched along, as a streak;
  /// zero draws it round. A spark is a streak, an ember is round.
  float stretch = 0.0f;
  /// How fast it turns, in degrees a second, either way. Every particle
  /// starts at an angle of its own, so a burst of them never turns as one.
  /// A streaking particle is turned by its motion instead, and ignores it.
  float spin = 0.0f;
  /// A soft disc, or a puff broken up by noise.
  FxParticleShape shape = FxParticleShape::DISC;
  /// Whether the scene's lights reach it.
  FxParticleLighting lighting = FxParticleLighting::EMISSIVE;

  /// Two looks are equal when every number of them is, to the last bit.
  bool operator==(const FxParticleLook&) const = default;
};

}  // namespace eng
