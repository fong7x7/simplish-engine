#pragma once

#include <cstdint>

namespace eng {

/// Snapshot of controller state at the current frame.
/// Returned by pollXboxInput() for each connected controller.
struct XboxControllerState {
  /// Left thumbstick horizontal axis [-1.0, 1.0].
  float left_stick_x = 0.0f;
  /// Left thumbstick vertical axis [-1.0, 1.0].
  float left_stick_y = 0.0f;
  /// Right thumbstick horizontal axis [-1.0, 1.0].
  float right_stick_x = 0.0f;
  /// Right thumbstick vertical axis [-1.0, 1.0].
  float right_stick_y = 0.0f;
  /// Left trigger analog value [0.0, 1.0].
  float left_trigger = 0.0f;
  /// Right trigger analog value [0.0, 1.0].
  float right_trigger = 0.0f;
  /// Bitmask of currently pressed XboxButton values.
  uint32_t buttons = 0;
  /// True if this controller is physically connected.
  bool connected = false;
};

}  // namespace eng
