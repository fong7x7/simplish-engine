#pragma once

/// @file input-method.h
/// @brief Which kind of device the player last used.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng::input {

/// The kind of device the player is using now — whatever they last
/// touched. What a prompt shows ("Enter" or "A"), whether menus lead with a
/// focus ring or a cursor, and whether the mouse aims all follow it.
enum class InputMethod : uint8_t {
  POINTER,   ///< The mouse, or a touch surface acting as one
  KEYBOARD,  ///< Keys
  GAMEPAD,   ///< A pad's buttons or sticks
};

}  // namespace eng::input
