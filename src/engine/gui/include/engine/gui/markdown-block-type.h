#pragma once

/// @file markdown-block-type.h
/// @brief Block-level element types for the Markdown AST.
/// @par Threading Main thread only.

#include <cstdint>

namespace eng::gui {

/// Discriminator for block-level Markdown elements.
enum class MarkdownBlockType : uint8_t {
  /// Plain text paragraph separated by blank lines.
  PARAGRAPH,
  /// ATX heading (# / ## / ###).
  HEADING,
  /// Fenced code block (triple backtick).
  CODE_BLOCK,
  /// Unordered list (- or * prefix).
  BULLET_LIST,
  /// Ordered list (N. prefix).
  ORDERED_LIST,
  /// Single item within a list block.
  LIST_ITEM,
  /// Block quotation (> prefix).
  BLOCKQUOTE,
  /// Thematic break (--- / *** / ___).
  HORIZONTAL_RULE,
  /// GFM-style table (header + separator + data rows).
  TABLE,
};

}  // namespace eng::gui
