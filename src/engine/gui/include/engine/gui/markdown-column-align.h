#pragma once

/// @file markdown-column-align.h
/// @brief Column alignment for Markdown table cells.
/// @par Threading Main thread only.

#include <cstdint>

namespace eng::gui {

/// Column alignment derived from GFM table separator row syntax.
enum class MarkdownColumnAlign : uint8_t {
  /// Left-aligned (default, `---` or `:---`).
  LEFT,
  /// Center-aligned (`:---:`).
  CENTER,
  /// Right-aligned (`---:`).
  RIGHT,
};

}  // namespace eng::gui
