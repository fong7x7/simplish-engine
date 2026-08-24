#pragma once

#include "shaped-glyph.h"
#include "text-line.h"

#include <vector>

namespace eng {

/// @thread_safety Main thread only.
struct ShapedRun {
  /// Font face used for this run.
  uint32_t face_id = 0;
  /// Positioned glyphs produced by HarfBuzz shaping.
  std::vector<ShapedGlyph> glyphs{};
  /// Total horizontal advance of all glyphs in this run.
  float total_advance = 0.0f;

  /// Greedy line breaks at max_width (logical pixels).
  [[nodiscard]] std::vector<TextLine> breakLines(float max_width) const;
};

}  // namespace eng
