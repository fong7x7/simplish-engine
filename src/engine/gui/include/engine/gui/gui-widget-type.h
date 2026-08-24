#pragma once

/// @file gui-widget-type.h
/// @brief Widget kind for theming, layout, and tree node factory.
/// @par Threading Main thread only.

#include <cstdint>

namespace eng {

enum class GuiWidgetType : uint8_t {
  /// Generic container panel.
  PANEL,
  /// Static text label.
  TEXT,
  /// Clickable button.
  BUTTON,
  /// Single-line text input field.
  TEXT_INPUT,
  /// Multi-line text area with word wrapping.
  TEXT_AREA,
  /// Scrollable container for overflow content.
  SCROLL_CONTAINER,
  /// Image display widget.
  IMAGE,
  /// Application-defined custom widget.
  CUSTOM,
};

}  // namespace eng
