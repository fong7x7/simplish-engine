#pragma once

/// @file gui-progress-bar.h
/// @brief How far along something is.
/// @par Threading
/// Main thread only.

#include "gui-widget.h"

namespace eng {

/// A progress bar: a rounded track filled from the left by `value`, or —
/// with `indeterminate` — a segment sweeping across while the end is not
/// known. Not focusable.
/// @thread_safety Main thread only.
class GuiProgressBar : public GuiWidget {
public:
  /// An empty bar.
  GuiProgressBar();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Sweep an indeterminate bar along.
  void update(const GuiDrawContext& ctx, float dt) override;

  /// The track and its fill.
  void render(const GuiDrawContext& ctx) const override;

  /// No width of its own; `thickness` high.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// How far along, 0 to 1.
  float value = 0.0f;
  /// Whether the end is unknown, so it sweeps instead.
  bool indeterminate = false;
  /// Its height.
  float thickness = 6.0f;

private:
  /// Draw an indeterminate bar's segment in @p fill.
  void drawSweep(const GuiDrawContext& ctx, const GuiColor& fill) const;

  /// Where the sweep is, 0 to 1, for an indeterminate bar.
  float phase_ = 0.0f;
};

}  // namespace eng
