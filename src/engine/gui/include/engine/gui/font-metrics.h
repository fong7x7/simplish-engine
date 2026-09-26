#pragma once

/// @file font-metrics.h
/// @brief A font's vertical measurements at one size.
/// @par Threading
/// Immutable value type.

namespace eng {

/// A face's vertical metrics at one text size, in layout pixels.
struct FontMetrics {
  /// Baseline to the top of the tallest glyph.
  float ascender = 0.0f;
  /// Baseline to the bottom of the lowest glyph (negative).
  float descender = 0.0f;
  /// The font's recommended distance between baselines.
  float line_height = 0.0f;
};

}  // namespace eng
