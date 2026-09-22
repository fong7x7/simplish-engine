#include <engine/gui/gui-nav-buttons.h>
#include <engine/input/gamepad-button-label.h>

namespace eng {

namespace {

  using input::GamepadButton;
  using input::GamepadFamily;

  /// One command and the button that asks for it.
  struct NavButton {
    /// The command.
    GuiNavCommand command;
    /// The button, by position.
    GamepadButton button;
  };

  /// Every command, by position, on a pad that confirms with the bottom
  /// button; Nintendo swaps the first two.
  constexpr NavButton NAV_BUTTONS[] = {
      {GuiNavCommand::CONFIRM, GamepadButton::SOUTH},
      {GuiNavCommand::CANCEL, GamepadButton::EAST},
      {GuiNavCommand::UP, GamepadButton::DPAD_UP},
      {GuiNavCommand::DOWN, GamepadButton::DPAD_DOWN},
      {GuiNavCommand::LEFT, GamepadButton::DPAD_LEFT},
      {GuiNavCommand::RIGHT, GamepadButton::DPAD_RIGHT},
      {GuiNavCommand::NEXT, GamepadButton::RIGHT_SHOULDER},
      {GuiNavCommand::PREVIOUS, GamepadButton::LEFT_SHOULDER},
  };

  /// @p button with South and East swapped on a Nintendo pad.
  GamepadButton byConvention(GamepadButton button, GamepadFamily family) {
    if (family != GamepadFamily::NINTENDO) {
      return button;
    }
    if (button == GamepadButton::SOUTH) {
      return GamepadButton::EAST;
    }
    return button == GamepadButton::EAST ? GamepadButton::SOUTH : button;
  }

}  // namespace

input::GamepadButton guiNavButton(GuiNavCommand command,
                                  input::GamepadFamily family) {
  for (const NavButton& entry : NAV_BUTTONS) {
    if (entry.command == command) {
      return byConvention(entry.button, family);
    }
  }
  return GamepadButton::SOUTH;
}

std::optional<GuiNavCommand> guiNavCommandFor(input::GamepadButton button,
                                              input::GamepadFamily family) {
  // The swap is its own inverse, so undoing it is doing it again.
  const GamepadButton positional = byConvention(button, family);
  for (const NavButton& entry : NAV_BUTTONS) {
    if (entry.button == positional) {
      return entry.command;
    }
  }
  return std::nullopt;
}

std::string_view guiNavPrompt(GuiNavCommand command,
                              input::GamepadFamily family) {
  return input::gamepadButtonLabel(guiNavButton(command, family), family);
}

}  // namespace eng
