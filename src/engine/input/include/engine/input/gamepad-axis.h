#pragma once

/// @file gamepad-axis.h
/// @brief The analog axes a gamepad has.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng::input {

/// One analog axis on a gamepad. Sticks run -1 to 1 with +Y down, as the
/// screen does; triggers run 0 to 1.
///
/// The order is SDL3's `SDL_GamepadAxis`, and the SDL backend asserts it.
enum class GamepadAxis : uint8_t {
  LEFT_X,         ///< Left stick, +1 right
  LEFT_Y,         ///< Left stick, +1 down
  RIGHT_X,        ///< Right stick, +1 right
  RIGHT_Y,        ///< Right stick, +1 down
  LEFT_TRIGGER,   ///< LT, L2, ZL; 0 released
  RIGHT_TRIGGER,  ///< RT, R2, ZR; 0 released
  COUNT,          ///< Number of axes; not an axis
};

/// Number of gamepad axes.
inline constexpr std::size_t GAMEPAD_AXIS_COUNT =
    static_cast<std::size_t>(GamepadAxis::COUNT);

}  // namespace eng::input
