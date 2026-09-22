#pragma once

/// @file input-source-kind.h
/// @brief What sort of control an input binding reads.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng::input {

/// Which device, and which part of it, an `InputSource` names.
enum class InputSourceKind : uint8_t {
  /// A key, by the platform's key symbol.
  KEY,
  /// A gamepad button, by `GamepadButton`.
  GAMEPAD_BUTTON,
  /// A gamepad axis pushed towards +1, by `GamepadAxis`. Triggers are only
  /// ever positive.
  GAMEPAD_AXIS_POSITIVE,
  /// A gamepad axis pushed towards -1, by `GamepadAxis`.
  GAMEPAD_AXIS_NEGATIVE,
};

}  // namespace eng::input
