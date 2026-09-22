#pragma once

/// @file gamepad-state.h
/// @brief What one gamepad's buttons and axes read right now.
/// @par Threading
/// Main-thread-only.

#include <array>
#include <cstdint>
#include <engine/input/gamepad-axis.h>
#include <engine/input/gamepad-button.h>

namespace eng::input {

/// One gamepad's raw state: which buttons are down and where each axis
/// sits, before any deadzone. The platform's pad backend
/// (`platform/input`) writes it once per frame; bindings read it.
class GamepadState {
public:
  /// Mark @p button down.
  void press(GamepadButton button);

  /// Mark @p button up.
  void release(GamepadButton button);

  /// Whether @p button is down.
  [[nodiscard]] bool held(GamepadButton button) const;

  /// Set @p axis to @p value, clamped to the axis's range.
  void setAxis(GamepadAxis axis, float value);

  /// Where @p axis sits, raw.
  [[nodiscard]] float axis(GamepadAxis axis) const {
    return axes_[static_cast<std::size_t>(axis)];
  }

  /// Everything released and centred — for a pad whose events stop
  /// arriving, as when the window loses focus.
  void clear() { *this = GamepadState{}; }

private:
  /// One bit per `GamepadButton`, set while it is down.
  uint32_t buttons_ = 0;
  /// Each `GamepadAxis`'s raw value.
  std::array<float, GAMEPAD_AXIS_COUNT> axes_{};
};

}  // namespace eng::input
