#pragma once

#include "gui-button-style.h"
#include "gui-panel.h"
#include "gui-widget-type.h"

#include <string_view>

namespace eng {

/// A clickable button with label and rounded corners.
/// Extends GuiPanel for background fill, rounded corners, and borders.
/// Renders with hover highlight and centered text.
/// @thread_safety Main thread only.
class GuiButton : public GuiPanel {
public:
  GuiButton() { widget_type = GuiWidgetType::BUTTON; }

  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render this button (fill + centered label).
  void render(const GuiDrawContext& ctx) const override;

  /// Text displayed on the button.
  std::string_view label{};
  /// Visual styling.
  GuiButtonStyle style{};
};

}  // namespace eng
