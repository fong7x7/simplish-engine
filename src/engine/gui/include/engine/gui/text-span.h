#pragma once

#include "text-style.h"

#include <cstdint>

namespace eng {

/// Sentinel value indicating the font size should be inherited from the parent.
constexpr float FONT_SIZE_INHERIT = -1.0f;

/// @thread_safety Main thread only.
struct TextSpan {
  /// Start byte offset within the rich text string.
  uint32_t start = 0;
  /// End byte offset (exclusive) within the rich text string.
  uint32_t end = 0;
  /// Text style for this span (bold, italic, etc.).
  TextStyle style = TextStyle::NORMAL;
  /// Text colour as packed RGBA for this span.
  uint32_t color = 0xFFFFFFFF;
  /// Font size override in pixels (FONT_SIZE_INHERIT = inherit from parent).
  float font_size = FONT_SIZE_INHERIT;
};

}  // namespace eng
