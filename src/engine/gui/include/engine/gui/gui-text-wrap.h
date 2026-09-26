#pragma once

/// @file gui-text-wrap.h
/// @brief Whether text breaks onto more lines to fit its width.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// Whether text wraps.
enum class GuiTextWrap : uint8_t {
  /// One line; only a newline breaks it.
  NONE,
  /// Broken at spaces to fit the width — within a word too, when a word
  /// alone is wider — and at newlines.
  WORD,
};

}  // namespace eng
