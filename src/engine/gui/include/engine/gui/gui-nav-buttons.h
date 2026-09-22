#pragma once

/// @file gui-nav-buttons.h
/// @brief Which pad button does each menu command, and what to call it.
/// @par Threading
/// Pure functions.

#include "gui-nav-command.h"

#include <engine/input/gamepad-button.h>
#include <engine/input/gamepad-family.h>
#include <optional>
#include <string_view>

namespace eng {

/// The button that asks for @p command on a @p family pad. Confirm and
/// cancel follow the family's own convention — the bottom button confirms
/// and the right one cancels, except on a Nintendo pad, where the right
/// one, labelled A, confirms — so a player's thumb does what it does in
/// every other game on that pad. Directions are the d-pad's; NEXT and
/// PREVIOUS the shoulders.
[[nodiscard]] input::GamepadButton guiNavButton(GuiNavCommand command,
                                                input::GamepadFamily family);

/// The command @p button asks for on a @p family pad, or nothing — the
/// inverse of `guiNavButton`.
[[nodiscard]] std::optional<GuiNavCommand>
guiNavCommandFor(input::GamepadButton button, input::GamepadFamily family);

/// What a prompt calls the button for @p command on a @p family pad: "A"
/// to confirm on Xbox and Nintendo pads alike, "Cross" on PlayStation.
[[nodiscard]] std::string_view guiNavPrompt(GuiNavCommand command,
                                            input::GamepadFamily family);

}  // namespace eng
