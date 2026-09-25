#pragma once

/// @file layout-size.h
/// @brief A width and height, as the layout's measure pass reports them.
/// @par Threading
/// Immutable value type.

namespace eng {

/// A size in logical pixels: what a widget measures, before it is placed.
struct LayoutSize {
  /// Width in logical pixels.
  float w = 0.0f;
  /// Height in logical pixels.
  float h = 0.0f;
};

}  // namespace eng
