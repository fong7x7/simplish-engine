#pragma once

#include "gui-color.h"
#include "gui-widget.h"

namespace eng {

/// A filled rectangular panel (optionally with rounded corners and border).
/// Non-interactive by default (no on_click), but can be used as a container.
/// Base class for widgets that need a filled background (button, text input,
/// dropdown). Subclasses call renderPanel() to draw background + border, then
/// draw their own content on top.
/// @thread_safety Main thread only.
class GuiPanel : public GuiWidget {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render this panel (filled rect + optional border).
  void render(const GuiDrawContext& ctx) const override;

  /// Parameters for renderPanel().
  struct RenderPanelParams {
    /// Draw context.
    const GuiDrawContext& ctx;
    /// Background fill color.
    const GuiColor& fill;
  };

  /// Draw filled background and optional border using the given fill color.
  /// Respects corner_radius, border_color, and border_width.
  void renderPanel(const RenderPanelParams& params) const;

  /// Background fill color.
  GuiColor fill_color{};
  /// Border line color.
  GuiColor border_color{};
  /// Corner radius in pixels (0 = sharp corners).
  float corner_radius = 0.0f;
  /// Border width in pixels (0 = no border).
  float border_width = 0.0f;
};

}  // namespace eng
