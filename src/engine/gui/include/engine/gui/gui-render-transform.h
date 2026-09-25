#pragma once

/// @file gui-render-transform.h
/// @brief A uniform scale then an offset, applied to what is drawn.
/// @par Threading
/// Immutable value type.

#include "gui-rect.h"

namespace eng {

/// Where a subtree is drawn relative to where it was laid out: every point
/// is scaled by `scale` about the origin, then moved by the offset. A
/// widget's `render_scale` (about its centre) and `render_offset` compose
/// into one of these for it and its children. Like a CSS transform it
/// moves pixels only: layout and hit testing still use the untransformed
/// rects.
struct GuiRenderTransform {
  /// Uniform scale.
  float scale = 1.0f;
  /// Horizontal offset after scaling, in layout pixels.
  float offset_x = 0.0f;
  /// Vertical offset after scaling, in layout pixels.
  float offset_y = 0.0f;

  /// Where @p x lands.
  [[nodiscard]] float mapX(float x) const;
  /// Where @p y lands.
  [[nodiscard]] float mapY(float y) const;
  /// Where @p rect lands.
  [[nodiscard]] Rect map(const Rect& rect) const;
  /// This transform applied after @p inner: a child's, drawn inside this.
  [[nodiscard]] GuiRenderTransform after(const GuiRenderTransform& inner) const;
  /// Scale by @p factor about @p box's centre, then move by (@p dx, @p dy)
  /// — the transform a widget's render_scale and render_offset make.
  [[nodiscard]] static GuiRenderTransform about(const Rect& box, float factor,
                                                float dx, float dy);
};

}  // namespace eng
