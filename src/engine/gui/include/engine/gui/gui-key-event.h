#pragma once

/// @file gui-key-event.h
/// @brief Platform-agnostic key up/down with modifiers.
/// @par Threading Main thread only.

#include <cstdint>

namespace eng {

/// @brief Platform-agnostic key event with modifier state.
/// @thread_safety Immutable value type.
struct GuiKeyEvent {
  /// Platform-independent virtual key code.
  uint32_t keycode = 0;
  /// Hardware scan code.
  uint32_t scancode = 0;
  /// True if the key is pressed, false if released.
  bool pressed = false;
  /// True if this is a key repeat event.
  bool repeat = false;
  /// True if the Shift modifier is held.
  bool shift = false;
  /// True if the Ctrl modifier is held.
  bool ctrl = false;
  /// True if the Alt modifier is held.
  bool alt = false;
  /// True if the GUI/Super/Cmd modifier is held.
  bool gui = false;
};

}  // namespace eng
