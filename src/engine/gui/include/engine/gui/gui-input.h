#pragma once

/// @file gui-input.h
/// @brief Hit-test result type; input routing lives on `GuiWidgetTree`
/// (gui-widget-tree.h).
/// @par Threading Main thread only.

#include "gui-key-event.h"
#include "gui-mouse-event.h"
#include "gui-widget-id.h"

#include <cstdint>
#include <string_view>

namespace eng {

struct HitTestResult {
  /// ID of the widget under the hit test coordinate (0 if none).
  GuiWidgetId widget_id = GUI_WIDGET_ID_INVALID;
  /// X coordinate in the hit widget's local space.
  float local_x = 0.0f;
  /// Y coordinate in the hit widget's local space.
  float local_y = 0.0f;
};

/// Tab-order direction for `GuiWidgetTree::advanceFocus`.
enum class FocusTraversalDirection : uint8_t {
  FORWARD,
  REVERSE,
};

/// Vertical neighbour search axis for spatial focus moves.
enum class FocusSpatialSearch : uint8_t {
  UP,
  DOWN,
};

}  // namespace eng
