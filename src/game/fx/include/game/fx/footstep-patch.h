#pragma once

/// @file footstep-patch.h
/// @brief A box laid on the level that sounds like its own surface.
/// @par Threading
/// A value type.

#include <engine/math/vec3.h>
#include <game/content/footstep-surface.h>

namespace eng::game {

/// A prop's box, and the surface a step inside its footprint lands on in
/// place of whatever the ground is painted with: a rug, a wooden deck, a
/// metal grate.
struct FootstepPatch {
  /// The box's lowest corner, in world tiles.
  Vec3 min{};
  /// The box's highest corner, in world tiles.
  Vec3 max{};
  /// What a step on it sounds like.
  FootstepSurface surface = FootstepSurface::GROUND;
};

}  // namespace eng::game
