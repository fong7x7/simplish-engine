#pragma once

/// @file step-set-stride.h
/// @brief How far a set of feet walks between steps.
/// @par Threading
/// Pure.

#include <game/content/step-set.h>

namespace eng::game {

/// How far @p steps walks between one step and the next, in tiles. Content,
/// not presentation, so the simulation's steps — which game logic hears —
/// and the footsteps a playtest sounds fall at the same stride.
[[nodiscard]] float stepSetStride(StepSet steps);

}  // namespace eng::game
