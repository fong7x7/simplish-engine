#pragma once

/// @file gamepad-button-label.h
/// @brief What a button or trigger is printed with on each family's pad.
/// @par Threading
/// Pure functions over constant tables.

#include <engine/input/gamepad-axis.h>
#include <engine/input/gamepad-button.h>
#include <engine/input/gamepad-family.h>
#include <string_view>

namespace eng::input {

/// The name printed on @p button on a @p family pad, for prompts: South is
/// "A" on an Xbox pad, "Cross" on a PlayStation one, and "B" on a Nintendo
/// one. A generic pad is labelled as an Xbox pad, as most are.
[[nodiscard]] std::string_view gamepadButtonLabel(GamepadButton button,
                                                  GamepadFamily family);

/// The name of @p axis on a @p family pad: "RT", "R2" or "ZR" for the right
/// trigger, "Left Stick" for either direction of the left stick.
[[nodiscard]] std::string_view gamepadAxisLabel(GamepadAxis axis,
                                                GamepadFamily family);

}  // namespace eng::input
