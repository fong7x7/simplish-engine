#pragma once

/// @file markdown-block.h
/// @brief Block-level node in the Markdown AST.
/// @par Threading Main thread only.

#include "markdown-block-type.h"
#include "markdown-inline.h"
#include "markdown-table-data.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eng::gui {

/// One block-level element produced by the Markdown parser.
/// Text-bearing blocks (paragraphs, headings, list items) carry inline spans.
/// Container blocks (lists, blockquotes) carry child blocks.
/// @thread_safety Main thread only.
struct MarkdownBlock {
  /// Kind of block element.
  MarkdownBlockType type = MarkdownBlockType::PARAGRAPH;
  /// Inline spans for text-bearing blocks (paragraphs, headings, list items).
  std::vector<MarkdownInline> inlines{};
  /// Nested child blocks for container elements (lists, blockquotes).
  std::vector<MarkdownBlock> children{};
  /// Language hint for fenced code blocks (empty if unspecified).
  std::string language{};
  /// Heading depth (1-3); 0 for non-heading blocks.
  uint8_t heading_level = 0;
  /// Starting number for ordered lists; 1 for other block types.
  uint8_t list_start = 1;
  /// Table data for TABLE blocks; nullopt for all other block types.
  std::optional<MarkdownTableData> table{};
};

}  // namespace eng::gui
