#pragma once

/// @file gui-focus-visibility.h
/// @brief Whether the focus ring is drawn.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// Whether the player is navigating by focus, so the focused widget is
/// ringed. Navigating shows it; the pointer moving hides it again, since a
/// ring beside the cursor would be two answers to "where am I".
enum class GuiFocusVisibility : uint8_t {
  /// Not drawn: the pointer is in charge.
  HIDDEN,
  /// Drawn around the focused widget.
  SHOWN,
};

}  // namespace eng
