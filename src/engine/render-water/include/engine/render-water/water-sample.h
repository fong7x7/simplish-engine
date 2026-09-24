#pragma once

/// @file water-sample.h
/// @brief The water at one point: depth, colour and clarity as numbers.
/// @par Threading A value type.

#include <engine/math/vec3.h>

namespace eng {

/// What the water is like at a point between cell corners: every field of
/// `WaterCell` as a number, blended from the corners round it.
struct WaterSample {
  /// How deep, in tiles; 0 where it is dry.
  float depth = 0.0f;
  /// Its colour, sRGB from 0 to 1.
  Vec3 color{};
  /// How opaque, 0 to 1.
  float opacity = 0.0f;
};

}  // namespace eng
