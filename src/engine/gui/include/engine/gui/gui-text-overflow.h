#pragma once

/// @file gui-text-overflow.h
/// @brief What a line of text too long for its box does.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// What happens to a line too long for its rect.
enum class GuiTextOverflow : uint8_t {
  /// Drawn past the edge (clip it with a parent's `childClipRect`).
  VISIBLE,
  /// Cut short with "…" so it fits: CSS's `text-overflow: ellipsis`.
  ELLIPSIS,
};

}  // namespace eng
