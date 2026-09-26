#pragma once

/// @file gui-wrapped-line.h
/// @brief One line of wrapped text.
/// @par Threading
/// Immutable value type.

#include <cstddef>

namespace eng {

/// A line of text as `GuiDrawContext::wrapText` broke it: a byte range of
/// the text and how wide it is set.
struct GuiWrappedLine {
  /// Byte offset of its first character.
  std::size_t begin = 0;
  /// Byte offset one past its last (spaces it broke at are left out).
  std::size_t end = 0;
  /// Its width in layout pixels.
  float width = 0.0f;
};

}  // namespace eng
