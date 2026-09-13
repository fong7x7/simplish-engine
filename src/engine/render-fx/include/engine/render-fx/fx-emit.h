#pragma once

/// @file fx-emit.h
/// @brief Where, which way and how big an effect goes off.
/// @par Threading
/// A value type.

#include <engine/math/vec3.h>

namespace eng {

/// Where an effect is played: the point it starts from, the way it points,
/// and a scale for the one effect played small and large — a blast sized
/// by its radius.
struct FxEmit {
  /// Where it starts, in tiles; z is the height above the floor.
  Vec3 at{};
  /// Which way it points: its bursts spread about this. Any length; zero
  /// throws every burst every way.
  Vec3 direction{};
  /// Multiplies every particle's speed and size and the flash's range.
  float scale = 1.0f;
};

}  // namespace eng
