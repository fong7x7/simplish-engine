#pragma once

/// @file audio-listener.h
/// @brief Where the world is heard from.
/// @par Threading
/// A value type.

#include <engine/math/vec2.h>
#include <engine/math/vec3.h>

namespace eng::audio {

/// The ears a world's sounds are placed around. In an isometric game they
/// are the player the camera follows, not the camera, and left and right are
/// the screen's: `right` is the world direction screen-right points along on
/// the ground, as `input::MoveBasis::right` is.
///
/// Distance is measured across the ground, in tiles. A sound within
/// `full_tiles` plays at full volume, one past `silent_tiles` is not heard,
/// and between the two it fades out along a square law, which falls quickly
/// near and slowly far as a real room does.
struct AudioListener {
  /// Where the ears are, in tiles.
  Vec3 at{};
  /// World direction of screen-right, unit length.
  Vec2 right{1.0F, 0.0F};
  /// Out to here, in tiles, a sound is at full volume.
  float full_tiles = 3.0F;
  /// Past here, in tiles, a sound is silent.
  float silent_tiles = 28.0F;
};

}  // namespace eng::audio
