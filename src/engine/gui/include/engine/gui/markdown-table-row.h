#pragma once

/// @file markdown-table-row.h
/// @brief One row (header or data) in a Markdown table.
/// @par Threading Main thread only.

#include "markdown-table-cell.h"

#include <vector>

namespace eng::gui {

/// A single row within a Markdown table (header or data).
/// @thread_safety Main thread only.
struct MarkdownTableRow {
  /// Cells in this row, one per column.
  std::vector<MarkdownTableCell> cells{};
};

}  // namespace eng::gui
