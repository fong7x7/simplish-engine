#pragma once

/// @file fx-volume.h
/// @brief A cloud of smoke the effects pass marches a ray through, rather
/// than a stack of flat particles.
/// @par Threading
/// A value type.

#include <engine/render-fx/fx-color.h>

namespace eng {

/// How much of its life a cloud spends fading in, so it swells out of
/// nothing rather than appearing whole.
inline constexpr float FX_VOLUME_FADE_IN = 0.15f;

/// One cloud of smoke: a box of procedural noise that light is absorbed
/// through, thickest at the middle and gone at its wall.
///
/// Where a particle is a flat quad with a picture on it, this is a shape
/// the fragment stage steps a ray through until the scene's depth stops
/// it — so it fills a doorway, wraps a crate, and thins out where it meets
/// the floor, none of which a billboard can do. It costs a march per
/// covered pixel, so a scene holds a handful of them, not thousands.
struct FxVolume {
  /// What the smoke adds and how much it hides at its thickest,
  /// premultiplied as every effects colour is.
  FxColor color{};
  /// How much light a tile of the thickest smoke absorbs. Around 2 is a
  /// cloud you cannot see through; below 0.5 is haze.
  float density = 2.0f;
  /// Half its width and depth at birth, in tiles.
  float radius = 1.0f;
  /// Half its height at birth, in tiles.
  float height = 1.0f;
  /// How many tiles a second every half-extent grows by.
  float growth = 0.0f;
  /// How many tiles a second the whole cloud drifts upwards.
  float rise = 0.0f;
  /// How long it lasts, in seconds.
  float life = 1.0f;

  /// Two clouds are equal when every number of them is, to the last bit.
  bool operator==(const FxVolume&) const = default;
};

}  // namespace eng
