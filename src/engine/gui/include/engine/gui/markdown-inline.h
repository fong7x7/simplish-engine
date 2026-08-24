#pragma once

/// @file markdown-inline.h
/// @brief One inline span within a Markdown block.
/// @par Threading Main thread only.

#include "markdown-inline-type.h"

#include <string>

namespace eng::gui {

/// A single inline element within a text-bearing Markdown block.
/// @thread_safety Main thread only.
struct MarkdownInline {
  /// Kind of inline element (text, bold, italic, code, link).
  MarkdownInlineType type = MarkdownInlineType::TEXT;
  /// Visible text content of this span.
  std::string text{};
  /// Link URL; empty for non-link types.
  std::string url{};
};

}  // namespace eng::gui
