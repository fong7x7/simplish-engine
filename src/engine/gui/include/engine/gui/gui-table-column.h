#pragma once

/// @file gui-table-column.h
/// @brief One column of a table.
/// @par Threading
/// Plain data.

#include "layout-engine.h"

#include <string>

namespace eng {

/// A `GuiTable` column: its header, width and alignment.
struct GuiTableColumn {
  /// Header text.
  std::string title{};
  /// Width in pixels; the user can drag it wider or narrower.
  float width = 120.0f;
  /// START, CENTER or END: numbers read best at the END.
  Align align = Align::START;
  /// Whether clicking its header asks for a sort.
  bool sortable = true;
};

}  // namespace eng
