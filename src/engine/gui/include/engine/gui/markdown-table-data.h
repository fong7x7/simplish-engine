#pragma once

/// @file markdown-table-data.h
/// @brief Structured data for a Markdown TABLE block.
/// @par Threading Main thread only.

#include "markdown-column-align.h"
#include "markdown-table-row.h"

#include <vector>

namespace eng::gui {

/// Table-specific metadata attached to a MarkdownBlock with type TABLE.
/// The separator row determines the authoritative column count; header and
/// data rows are normalised to match.
/// @thread_safety Main thread only.
struct MarkdownTableData {
  /// Per-column alignment parsed from the separator row.
  std::vector<MarkdownColumnAlign> columns{};
  /// Header row (always present in a valid GFM table).
  MarkdownTableRow header{};
  /// Data rows below the header (may be empty).
  std::vector<MarkdownTableRow> rows{};
};

}  // namespace eng::gui
