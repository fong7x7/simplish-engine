#pragma once

#include "gui-button-variant.h"
#include "gui-panel.h"
#include "gui-widget-type.h"

#include <string_view>

namespace eng {

/// A clickable button with a centred label, drawn in its theme variant's
/// look for the state it is in (hover, pressed, selected, disabled…),
/// blending between them.
/// @thread_safety Main thread only.
class GuiButton : public GuiPanel {
public:
  /// A button, which takes navigation focus.
  GuiButton();

  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render this button (fill + centered label).
  void render(const GuiDrawContext& ctx) const override;

  /// The label's width, one line high; padding makes the rest.
  [[nodiscard]] LayoutSize
  measureContent(const GuiDrawContext& ctx) const override;

  /// The theme's look for this button's `variant`.
  [[nodiscard]] const GuiStateStyles*
  themeStyles(const GuiTheme& theme) const override;

  /// Text displayed on the button.
  std::string_view label{};
  /// Which of the theme's button looks it takes; `state_styles` overrides
  /// it with a look of its own.
  GuiButtonVariant variant = GuiButtonVariant::NEUTRAL;
};

}  // namespace eng
