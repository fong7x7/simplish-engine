#pragma once

/// @file ui-anchor.h
/// @brief Where on the game's view a screen's root sits.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Where a screen's root node is placed on the view — at its own size,
/// inset by the screen's margin — or `FILL` to cover it.
enum class UiAnchor : uint8_t {
  /// In the middle.
  CENTER,
  /// Top edge, centred across.
  TOP,
  /// Bottom edge, centred across.
  BOTTOM,
  /// Left edge, centred up and down.
  LEFT,
  /// Right edge, centred up and down.
  RIGHT,
  /// Top-left corner.
  TOP_LEFT,
  /// Top-right corner.
  TOP_RIGHT,
  /// Bottom-left corner.
  BOTTOM_LEFT,
  /// Bottom-right corner.
  BOTTOM_RIGHT,
  /// The whole view.
  FILL,
};

}  // namespace eng::game
