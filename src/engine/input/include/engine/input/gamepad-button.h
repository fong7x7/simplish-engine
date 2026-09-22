#pragma once

/// @file gamepad-button.h
/// @brief The buttons a gamepad has, named by where they sit, not by label.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng::input {

/// One button on a gamepad, named for its position so one binding means
/// the same thumb movement on every pad: `SOUTH` is Xbox A, PlayStation
/// Cross, and Nintendo B.
///
/// The order is SDL3's `SDL_GamepadButton`, up to the touchpad, and the
/// SDL backend in `platform/input` asserts it; a console backend maps its
/// own pad onto the same names.
enum class GamepadButton : uint8_t {
  SOUTH,           ///< Bottom face button: A, Cross, Nintendo B
  EAST,            ///< Right face button: B, Circle, Nintendo A
  WEST,            ///< Left face button: X, Square, Nintendo Y
  NORTH,           ///< Top face button: Y, Triangle, Nintendo X
  BACK,            ///< View, Share or Create, Minus
  GUIDE,           ///< Xbox, PS, or Home
  START,           ///< Menu, Options, Plus
  LEFT_STICK,      ///< Pressing the left stick in
  RIGHT_STICK,     ///< Pressing the right stick in
  LEFT_SHOULDER,   ///< LB, L1, L
  RIGHT_SHOULDER,  ///< RB, R1, R
  DPAD_UP,         ///< D-pad up
  DPAD_DOWN,       ///< D-pad down
  DPAD_LEFT,       ///< D-pad left
  DPAD_RIGHT,      ///< D-pad right
  MISC,            ///< Xbox Share, DualSense mute, Switch Capture
  RIGHT_PADDLE_1,  ///< Upper right back paddle (Elite, DualSense Edge)
  LEFT_PADDLE_1,   ///< Upper left back paddle
  RIGHT_PADDLE_2,  ///< Lower right back paddle
  LEFT_PADDLE_2,   ///< Lower left back paddle
  TOUCHPAD,        ///< Pressing a PlayStation touchpad
  COUNT,           ///< Number of buttons; not a button
};

/// Number of gamepad buttons.
inline constexpr std::size_t GAMEPAD_BUTTON_COUNT =
    static_cast<std::size_t>(GamepadButton::COUNT);

}  // namespace eng::input
