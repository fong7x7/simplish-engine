#pragma once

/// @file markdown-inline-type.h
/// @brief Inline-level element types for the Markdown AST.
/// @par Threading Main thread only.

#include <cstdint>

namespace eng::gui {

/// Discriminator for inline-level Markdown elements within a block.
enum class MarkdownInlineType : uint8_t {
  /// Plain unformatted text.
  TEXT,
  /// Bold text (**text**).
  BOLD,
  /// Italic text (*text*).
  ITALIC,
  /// Bold and italic text (***text***).
  BOLD_ITALIC,
  /// Inline code span (`code`).
  INLINE_CODE,
  /// Hyperlink ([text](url)).
  LINK,
};

}  // namespace eng::gui
