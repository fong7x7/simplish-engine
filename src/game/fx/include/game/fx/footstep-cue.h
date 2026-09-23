#pragma once

/// @file footstep-cue.h
/// @brief One step somebody took.
/// @par Threading
/// A value type.

#include <engine/math/vec3.h>
#include <game/content/footstep-surface.h>
#include <game/content/step-set.h>

namespace eng::game {

/// A foot coming down: where, on what, and whose kind of foot — all a
/// footstep's sound is picked from (`footstepSound`).
struct FootstepCue {
  /// Where the foot landed, in world tiles.
  Vec3 at{};
  /// What the foot is.
  StepSet steps = StepSet::DEFAULT;
  /// What it landed on.
  FootstepSurface surface = FootstepSurface::GROUND;
};

}  // namespace eng::game
