#pragma once

/// @file gui-checkbox.h
/// @brief A tick box with a label.
/// @par Threading
/// Main thread only.

#include "gui-check-state.h"
#include "gui-widget.h"

#include <functional>
#include <string>

namespace eng {

/// A checkbox: a box that ticks and unticks on a click or CONFIRM, with
/// its label beside it; the whole row is the target. Takes focus.
/// @thread_safety Main thread only.
class GuiCheckbox : public GuiWidget {
public:
  /// An unchecked, focusable checkbox.
  GuiCheckbox();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Its box and label.
  void render(const GuiDrawContext& ctx) const override;

  /// The box, a gap, and the label, one line high.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// Toggle.
  bool handleClick(const GuiMouseEvent& event) override;

  /// CONFIRM toggles.
  bool handleNav(GuiNavCommand command) override;

  /// Flip it — unchecked or indeterminate to checked, checked to
  /// unchecked — and tell `on_change`.
  void toggle();

  /// Beside the box.
  std::string label{};
  /// Ticked, empty or a dash.
  GuiCheckState state = GuiCheckState::UNCHECKED;
  /// Called with whether it is checked after a toggle.
  std::function<void(bool)> on_change{};

private:
  /// Draw the label from @p x, centred up and down.
  void drawLabel(const GuiDrawContext& ctx, float x) const;
};

}  // namespace eng
