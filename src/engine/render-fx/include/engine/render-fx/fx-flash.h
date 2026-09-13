#pragma once

/// @file fx-flash.h
/// @brief A light that flares and dies away.
/// @par Threading
/// A value type.

#include <engine/math/vec3.h>

namespace eng {

/// A point light that is brightest when it starts and fades to nothing
/// over its life — a muzzle flash, the flare of a hit, a blast. It lights
/// meshes exactly as a placed point light does, through `MeshLight`.
struct FxFlash {
  /// Linear RGB tint, each component in [0, 1].
  Vec3 color{1.0f, 1.0f, 1.0f};
  /// Brightness at its start, as `MeshLight::intensity`: 1 lights a
  /// surface head-on as brightly as the built-in key light. Zero is no
  /// flash at all.
  float intensity = 0.0f;
  /// How far it reaches, in tiles.
  float range = 0.0f;
  /// How long it lasts, in seconds.
  float life = 0.0f;
};

}  // namespace eng
