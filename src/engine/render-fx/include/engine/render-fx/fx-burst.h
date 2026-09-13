#pragma once

/// @file fx-burst.h
/// @brief A handful of particles thrown out at once.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/render-fx/fx-particle-look.h>

namespace eng {

/// A burst: `count` particles, each thrown some way inside a cone about
/// the direction it is emitted in, at some speed, to live some time — each
/// drawn uniformly between its bounds — and all looking as `look` says.
struct FxBurst {
  /// How many particles.
  uint16_t count = 0;
  /// Half-angle of the cone they are thrown in, in degrees, about the
  /// direction they are emitted in. 180 throws them every way.
  float spread_degrees = 180.0f;
  /// Slowest a particle leaves, in tiles per second.
  float speed_min = 0.0f;
  /// Fastest a particle leaves, in tiles per second.
  float speed_max = 0.0f;
  /// Shortest a particle lives, in seconds.
  float life_min = 0.1f;
  /// Longest a particle lives, in seconds.
  float life_max = 0.1f;
  /// How every particle of the burst looks and moves.
  FxParticleLook look{};

  /// Two bursts are equal when every number of them is, to the last bit.
  bool operator==(const FxBurst&) const = default;
};

}  // namespace eng
