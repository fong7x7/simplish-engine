#pragma once

/// @file gamepad-actions.h
/// @brief A pad's sticks and buttons, through its bindings, as actions.
/// @par Threading
/// Pure functions.

#include <engine/input/action-values.h>
#include <engine/input/gamepad-state.h>
#include <engine/input/input-bindings.h>
#include <engine/math/vec2.h>

namespace eng::input {

/// @p stick with a radial @p deadzone taken off: zero inside it, and the
/// travel past it rescaled to run from 0 to full length, which is capped
/// at 1.
[[nodiscard]] Vec2 applyStickDeadzone(Vec2 stick, float deadzone);

/// @p trigger, 0 to 1, with @p deadzone taken off the bottom and the rest
/// rescaled to run 0 to 1.
[[nodiscard]] float applyTriggerDeadzone(float trigger, float deadzone);

/// @p pad after its deadzones, as @p bindings map it, offered into
/// @p values: a button bound to an action asks for it fully, and an axis
/// direction as far as it is pushed that way.
void offerGamepad(ActionValues& values, const GamepadState& pad,
                  const InputBindings& bindings);

}  // namespace eng::input
