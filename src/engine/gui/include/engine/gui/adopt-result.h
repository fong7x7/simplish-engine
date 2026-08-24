#pragma once

/// @file adopt-result.h
/// @brief Result of GuiWidgetTree::adoptSubtreeWithMap, carrying the new root
/// ID and an old-to-new ID mapping.
/// @par Threading Main thread only.

#include "gui-widget-id.h"

#include <unordered_map>

namespace eng {

/// Result of adoptSubtree when an ID mapping is requested.
struct AdoptResult {
  /// New root widget ID in the target tree.
  GuiWidgetId new_root = GUI_WIDGET_ID_INVALID;
  /// Mapping from source tree IDs to target tree IDs.
  std::unordered_map<GuiWidgetId, GuiWidgetId> id_map{};
};

}  // namespace eng
