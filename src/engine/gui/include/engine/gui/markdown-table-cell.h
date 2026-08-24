#pragma once

/// @file markdown-table-cell.h
/// @brief One cell in a Markdown table row.
/// @par Threading Main thread only.

#include "markdown-inline.h"

#include <vector>

namespace eng::gui {

/// A single cell within a Markdown table row.
/// Contains inline spans so cells support bold, italic, code, and links.
/// @thread_safety Main thread only.
struct MarkdownTableCell {
  /// Formatted inline content of this cell.
  std::vector<MarkdownInline> inlines{};
};

}  // namespace eng::gui
