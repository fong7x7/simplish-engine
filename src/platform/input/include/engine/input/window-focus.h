#pragma once

/// @file window-focus.h
/// @brief Whether the window the pads are read for has focus.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng::input {

/// Whether the game's window is the one the player is using. Pads are
/// shared by every window on the machine; one the player has left should
/// not keep walking their character.
enum class WindowFocus : uint8_t {
  /// The window has focus; pads are read.
  FOCUSED,
  /// It does not; every pad reads as resting until it does again.
  UNFOCUSED,
};

}  // namespace eng::input
