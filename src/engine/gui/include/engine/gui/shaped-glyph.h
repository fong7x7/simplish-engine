#pragma once

#include <cstdint>

namespace eng {

/// @thread_safety Main thread only.
struct ShapedGlyph {
  /// Font-specific glyph index from HarfBuzz shaping.
  uint32_t glyph_index = 0;
  /// Unicode codepoint of the original character.
  uint32_t codepoint = 0;
  /// Horizontal offset from the pen position.
  float x_offset = 0.0f;
  /// Vertical offset from the pen position.
  float y_offset = 0.0f;
  /// Horizontal advance to the next glyph position.
  float x_advance = 0.0f;
  /// Cluster index linking glyphs to source characters.
  uint32_t cluster = 0;
};

}  // namespace eng
