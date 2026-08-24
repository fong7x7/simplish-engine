#pragma once

/// @file gui-widget-id.h
/// @brief Stable identity for nodes in `GuiWidgetTree` /
/// `GuiWidget::widget_id`.
/// @par Threading Main thread only.

#include <cstdint>

namespace eng {

/// Opaque numeric id for a widget node in the retained tree.
using GuiWidgetId = uint64_t;
/// Sentinel meaning no widget / invalid reference.
inline constexpr GuiWidgetId GUI_WIDGET_ID_INVALID = 0;

}  // namespace eng
