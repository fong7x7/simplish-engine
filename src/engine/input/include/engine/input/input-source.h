#pragma once

/// @file input-source.h
/// @brief One physical control an action can be bound to.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/input/gamepad-axis.h>
#include <engine/input/gamepad-button.h>
#include <engine/input/input-source-kind.h>

namespace eng::input {

/// A key, a pad button, or one direction of a pad axis. The code is the
/// key symbol, or the `GamepadButton` or `GamepadAxis` cast to an integer.
struct InputSource {
  /// Which device and part `code` names.
  InputSourceKind kind = InputSourceKind::KEY;
  /// The key symbol, button or axis.
  uint32_t code = 0;

  /// The key whose platform symbol is @p key.
  [[nodiscard]] static constexpr InputSource key(uint32_t key) {
    return {InputSourceKind::KEY, key};
  }

  /// The pad button @p button.
  [[nodiscard]] static constexpr InputSource button(GamepadButton button) {
    return {InputSourceKind::GAMEPAD_BUTTON, static_cast<uint32_t>(button)};
  }

  /// @p axis pushed towards +1.
  [[nodiscard]] static constexpr InputSource positive(GamepadAxis axis) {
    return {InputSourceKind::GAMEPAD_AXIS_POSITIVE,
            static_cast<uint32_t>(axis)};
  }

  /// @p axis pushed towards -1.
  [[nodiscard]] static constexpr InputSource negative(GamepadAxis axis) {
    return {InputSourceKind::GAMEPAD_AXIS_NEGATIVE,
            static_cast<uint32_t>(axis)};
  }

  /// Two sources are the same control.
  friend constexpr bool operator==(const InputSource&,
                                   const InputSource&) = default;
};

}  // namespace eng::input
