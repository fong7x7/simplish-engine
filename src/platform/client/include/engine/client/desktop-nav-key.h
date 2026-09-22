#pragma once

/// @file desktop-nav-key.h
/// @brief One key press, as keyboard menu navigation needs to see it.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::client {

/// A key that went down, with what decides whether it navigates: Shift
/// turns Tab backwards, and a text field taking typing keeps the arrows,
/// Enter and Space for itself.
struct DesktopNavKey {
  /// The platform key symbol.
  uint32_t key = 0;
  /// Either Shift key is held.
  bool shift = false;
  /// A text field is taking typing.
  bool typing = false;
  /// The OS is repeating a held key rather than a new press.
  bool repeat = false;
};

}  // namespace eng::client
