#pragma once

/// @file gui-font-slant.h
/// @brief Upright or italic.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// Whether text is set upright or slanted.
enum class GuiFontSlant : uint8_t {
  /// Upright.
  NORMAL,
  /// Italic: the family's italic face when loaded, else upright.
  ITALIC,
};

}  // namespace eng
