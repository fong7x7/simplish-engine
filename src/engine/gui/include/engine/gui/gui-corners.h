#pragma once

/// @file gui-corners.h
/// @brief A radius for each corner of a rect.
/// @par Threading
/// Immutable value type.

namespace eng {

/// Corner radii in logical pixels, as CSS's `border-radius: tl tr br bl`.
struct GuiCorners {
  /// Top-left.
  float top_left = 0.0f;
  /// Top-right.
  float top_right = 0.0f;
  /// Bottom-right.
  float bottom_right = 0.0f;
  /// Bottom-left.
  float bottom_left = 0.0f;

  /// All four at @p radius.
  static constexpr GuiCorners all(float radius) {
    return {radius, radius, radius, radius};
  }
  /// The top two at @p radius, the bottom square: a tab, a sheet's top.
  static constexpr GuiCorners top(float radius) {
    return {radius, radius, 0.0f, 0.0f};
  }
  /// The bottom two at @p radius: a dropdown hanging from a bar.
  static constexpr GuiCorners bottom(float radius) {
    return {0.0f, 0.0f, radius, radius};
  }
};

}  // namespace eng
