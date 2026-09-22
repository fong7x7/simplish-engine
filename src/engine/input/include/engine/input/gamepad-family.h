#pragma once

/// @file gamepad-family.h
/// @brief Whose pad it is, for what its buttons are printed with.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng::input {

/// Which maker's layout a pad follows. Bindings never depend on it —
/// buttons are named by position — but what a prompt calls a button, and
/// which face button a menu confirms with, do.
enum class GamepadFamily : uint8_t {
  GENERIC,      ///< Unknown or third-party; labelled as an Xbox pad
  XBOX,         ///< Xbox 360, One, Series
  PLAYSTATION,  ///< DualShock 3 and 4, DualSense
  NINTENDO,     ///< Switch Pro, Joy-Cons alone or paired
};

}  // namespace eng::input
