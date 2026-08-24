#pragma once

/// @file markdown-parser.h
/// @brief Stateless parser: Markdown string to block AST.
/// @par Threading Main thread only.
///
/// Parses a subset of Markdown used in AI chat responses: bold, italic,
/// inline code, fenced code blocks, headings, lists, blockquotes, links,
/// and horizontal rules.  Malformed input degrades gracefully to plain text.

#include "markdown-block.h"

#include <string_view>
#include <vector>

namespace eng::gui {

/// Stateless Markdown parser producing a flat block AST.
/// @thread_safety Main thread only.
struct MarkdownParser {
  /// Parse a Markdown string into a list of block-level elements.
  /// Each block carries its inline spans (bold, italic, code, links).
  /// Returns an empty vector for empty input.
  /// Malformed Markdown degrades to PARAGRAPH blocks with TEXT inlines.
  static std::vector<MarkdownBlock> parse(std::string_view input);
};

}  // namespace eng::gui
