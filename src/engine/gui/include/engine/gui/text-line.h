#pragma once

#include <cstdint>

namespace eng {

// --- Text line ---

/// @thread_safety Main thread only.
struct TextLine {
  /// Index of the first glyph in this line within the shaped run.
  uint32_t start_index = 0;
  /// Index past the last glyph in this line.
  uint32_t end_index = 0;
  /// Total width of this line in pixels.
  float width = 0.0f;
  /// Maximum ascender height for this line.
  float ascender = 0.0f;
  /// Maximum descender depth for this line (negative).
  float descender = 0.0f;
};

}  // namespace eng
