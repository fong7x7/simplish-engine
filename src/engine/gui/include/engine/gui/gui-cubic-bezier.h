#pragma once

/// @file gui-cubic-bezier.h
/// @brief A CSS `cubic-bezier()` timing curve.
/// @par Threading
/// A value type; pure.

namespace eng {

/// A timing curve from (0, 0) to (1, 1) through two control points, as
/// CSS's `cubic-bezier(x1, y1, x2, y2)` defines it: `at(t)` is the eased
/// progress at the fraction @p t of the time. A `y` outside 0–1 overshoots.
struct GuiCubicBezier {
  /// The first control point's time.
  float x1 = 0.25f;
  /// The first control point's progress.
  float y1 = 0.1f;
  /// The second control point's time.
  float x2 = 0.25f;
  /// The second control point's progress.
  float y2 = 1.0f;

  /// The progress at time @p t, 0 to 1 (clamped): the curve's `y` where
  /// its `x` is @p t.
  [[nodiscard]] float at(float t) const;
};

/// CSS's `ease`: a quick start and a long settle. The default.
inline constexpr GuiCubicBezier GUI_BEZIER_EASE{0.25f, 0.1f, 0.25f, 1.0f};
/// Material's emphasized decelerate: most of the move at once, then a long,
/// soft arrival — for things entering.
inline constexpr GuiCubicBezier GUI_BEZIER_EMPHASIZED{0.05f, 0.7f, 0.1f, 1.0f};
/// A slight overshoot past the end, then back: a popover landing.
inline constexpr GuiCubicBezier GUI_BEZIER_BACK_OUT{0.34f, 1.56f, 0.64f, 1.0f};

}  // namespace eng
