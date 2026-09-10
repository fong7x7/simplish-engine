#pragma once

/// @file player-input-builder.h
/// @brief Turns held actions and an aim into the input a tick runs on.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <engine/input/held-actions.h>
#include <engine/input/move-basis.h>
#include <engine/math/vec2.h>
#include <engine/sim/player-input.h>

namespace eng::input {

/// A full-scale axis value in a `PlayerInput`.
inline constexpr int16_t INPUT_AXIS_MAX = 32767;

/// @p value, in [-1, 1], as a quantised axis: rounded to the nearest step
/// and clamped, so anything outside the range is full scale.
///
/// This is where the float stops. Everything after it — the lockstep wire,
/// the replay, the simulation — sees the integer, so two machines that
/// round a stick position differently still agree on what was sent.
[[nodiscard]] int16_t quantizeInputAxis(float value);

/// The input a tick runs on, from the actions @p held, the direction @p aim
/// points in world X and Y, and the camera's @p basis.
///
/// The movement actions are screen directions — up is up the screen — and
/// @p basis turns them into the world directions the input carries, so
/// movement follows the camera rather than the grid. A diagonal is scaled
/// so it is no faster than a straight line, and opposite directions held
/// together cancel. The aim is already a world direction; it is normalised
/// before it is quantised, so it is a direction whatever length it arrives
/// at, and a zero aim stays zero, which the game reads as "keep the aim you
/// had".
[[nodiscard]] sim::PlayerInput
makePlayerInput(const HeldActions& held, Vec2 aim, const MoveBasis& basis);

}  // namespace eng::input
