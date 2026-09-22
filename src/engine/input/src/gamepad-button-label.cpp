#include <array>
#include <engine/input/gamepad-button-label.h>

namespace eng::input {

namespace {

  /// Number of families with a label table.
  constexpr std::size_t FAMILY_COUNT = 4;

  /// Each family's button names, in `GamepadButton` order.
  using ButtonNames = std::array<std::string_view, GAMEPAD_BUTTON_COUNT>;
  /// Each family's axis names, in `GamepadAxis` order.
  using AxisNames = std::array<std::string_view, GAMEPAD_AXIS_COUNT>;

  constexpr ButtonNames XBOX_BUTTONS = {
      "A",          "B",          "X",           "Y",     "View", "Xbox",
      "Menu",       "LS",         "RS",          "LB",    "RB",   "D-pad Up",
      "D-pad Down", "D-pad Left", "D-pad Right", "Share", "P1",   "P3",
      "P2",         "P4",         "Touchpad",
  };

  constexpr ButtonNames PLAYSTATION_BUTTONS = {
      "Cross",
      "Circle",
      "Square",
      "Triangle",
      "Create",
      "PS",
      "Options",
      "L3",
      "R3",
      "L1",
      "R1",
      "D-pad Up",
      "D-pad Down",
      "D-pad Left",
      "D-pad Right",
      "Mute",
      "Right Paddle",
      "Left Paddle",
      "Right Paddle 2",
      "Left Paddle 2",
      "Touchpad",
  };

  // Nintendo prints A on the right face button and B on the bottom one:
  // positional South is labelled B.
  constexpr ButtonNames NINTENDO_BUTTONS = {
      "B",
      "A",
      "Y",
      "X",
      "-",
      "Home",
      "+",
      "LS",
      "RS",
      "L",
      "R",
      "D-pad Up",
      "D-pad Down",
      "D-pad Left",
      "D-pad Right",
      "Capture",
      "Right Paddle",
      "Left Paddle",
      "Right Paddle 2",
      "Left Paddle 2",
      "Touchpad",
  };

  constexpr AxisNames XBOX_AXES = {"Left Stick",  "Left Stick", "Right Stick",
                                   "Right Stick", "LT",         "RT"};
  constexpr AxisNames PLAYSTATION_AXES = {
      "Left Stick", "Left Stick", "Right Stick", "Right Stick", "L2", "R2"};
  constexpr AxisNames NINTENDO_AXES = {
      "Left Stick", "Left Stick", "Right Stick", "Right Stick", "ZL", "ZR"};

  /// Each family's tables, in `GamepadFamily` order; generic uses Xbox's.
  constexpr std::array<const ButtonNames*, FAMILY_COUNT> BUTTONS = {
      &XBOX_BUTTONS, &XBOX_BUTTONS, &PLAYSTATION_BUTTONS, &NINTENDO_BUTTONS};
  constexpr std::array<const AxisNames*, FAMILY_COUNT> AXES = {
      &XBOX_AXES, &XBOX_AXES, &PLAYSTATION_AXES, &NINTENDO_AXES};

}  // namespace

std::string_view gamepadButtonLabel(GamepadButton button,
                                    GamepadFamily family) {
  const auto index = static_cast<std::size_t>(button);
  return index < GAMEPAD_BUTTON_COUNT
             ? (*BUTTONS[static_cast<std::size_t>(family)])[index]
             : std::string_view{};
}

std::string_view gamepadAxisLabel(GamepadAxis axis, GamepadFamily family) {
  const auto index = static_cast<std::size_t>(axis);
  return index < GAMEPAD_AXIS_COUNT
             ? (*AXES[static_cast<std::size_t>(family)])[index]
             : std::string_view{};
}

}  // namespace eng::input
