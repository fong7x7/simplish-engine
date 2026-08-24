#pragma once

#include "gui-color.h"
#include "gui-widget.h"

#include <cstdint>
#include <string_view>

namespace eng {

/// Text alignment mode for labels.
enum class GuiLabelAlign : uint8_t {
  /// Left-aligned at rect.x, rect.y.
  LEFT,
  /// Centered both horizontally and vertically within rect.
  CENTER,
  /// Centered horizontally at rect center X, drawn at rect.y.
  H_CENTER,
};

/// A text label rendered at a position or centered within a rect.
/// Non-interactive by default.
/// @thread_safety Main thread only.
class GuiLabel : public GuiWidget {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render this label.
  void render(const GuiDrawContext& ctx) const override;

  /// Text string to render.
  std::string_view text{};
  /// Text color.
  GuiColor color{};
  /// Alignment mode.
  GuiLabelAlign align = GuiLabelAlign::LEFT;
};

}  // namespace eng
