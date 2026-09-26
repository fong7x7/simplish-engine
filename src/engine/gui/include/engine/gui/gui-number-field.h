#pragma once

/// @file gui-number-field.h
/// @brief A number you drag, step or scroll to change.
/// @par Threading
/// Main thread only.

#include "gui-widget.h"

#include <functional>
#include <string>

namespace eng {

/// A number field: its value, formatted, between − and + steppers, in the
/// theme's field look. Drag across it to scrub the value, click a stepper
/// or turn the wheel over it to step it, or — with focus — press LEFT and
/// RIGHT. Always within [min, max], rounded to `decimals`.
/// @thread_safety Main thread only.
class GuiNumberField : public GuiWidget {
public:
  /// A focusable field at 0.
  GuiNumberField();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// The theme's field look.
  [[nodiscard]] const GuiStateStyles*
  themeStyles(const GuiTheme& theme) const override;

  /// Its box, steppers and value.
  void render(const GuiDrawContext& ctx) const override;

  /// Wide enough for its widest value and the steppers, a line high.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// A stepper steps; anywhere else starts a scrub. Captures the pointer.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Scrub by how far the pointer has moved.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// End a scrub.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// The wheel steps it.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// LEFT and RIGHT step it.
  bool handleNav(GuiNavCommand command) override;

  /// Set it to @p v — clamped and rounded — and tell `on_change` if that
  /// changed it.
  void setValue(double v);

  /// Its value formatted with `decimals` places and `suffix`.
  [[nodiscard]] std::string text() const;

  /// The number.
  double value = 0.0;
  /// Smallest it may be.
  double min = 0.0;
  /// Largest it may be.
  double max = 100.0;
  /// How much a stepper, the wheel or LEFT / RIGHT changes it.
  double step = 1.0;
  /// Decimal places shown and kept.
  int decimals = 0;
  /// Pixels of drag per step while scrubbing.
  float pixels_per_step = 4.0f;
  /// After the number: "%", " px", "°".
  std::string suffix{};
  /// Called with the new value after it changes.
  std::function<void(double)> on_change{};

private:
  /// Draw the − and + steppers with their text's top at @p y.
  void drawSteppers(const GuiDrawContext& ctx, float y) const;

  /// Where a scrub started across, and the value then.
  float scrub_x_ = 0.0f;
  /// The value when the scrub started.
  double scrub_from_ = 0.0;
  /// Whether a scrub is under way.
  bool scrubbing_ = false;
};

}  // namespace eng
