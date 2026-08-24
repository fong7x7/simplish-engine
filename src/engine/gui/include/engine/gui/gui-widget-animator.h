#pragma once

/// @file gui-widget-animator.h
/// @brief Per-widget animation list with tick, add, and cancel operations.
/// @threading Main-thread only.

#include "gui-animation.h"
#include "gui-color.h"

#include <vector>

namespace eng {

class GuiWidget;  // NOLINT(no-forward-decl) breaks circular include with
                  // gui-widget.h

/// Manages a list of active animations for a single widget.
/// Each widget owns one GuiWidgetAnimator; widget destruction auto-cleans.
/// @threading Main-thread only.
struct GuiWidgetAnimator {
  /// Active animations running on the owning widget.
  std::vector<GuiAnimation> animations{};

  /// Tick all animations by dt seconds. Applies interpolated values to
  /// the widget. Returns true if any animations are still active.
  bool tick(GuiWidget& widget, float dt);

  /// Add an animation. If one targeting the same property already exists,
  /// it is replaced.
  void add(const GuiAnimation& anim);

  /// Cancel and remove all active animations.
  void cancelAll();

  /// True if any animations are currently active.
  bool hasActiveAnimations() const;

  /// Apply an interpolated scalar to the appropriate widget field.
  static void applyScalar(GuiWidget& widget, GuiAnimProperty prop, float val);

  /// Apply an interpolated color to the appropriate panel field.
  static void applyColor(GuiWidget& widget, GuiAnimProperty prop,
                         const GuiColor& val);

  /// Advance one animation and apply its value to the widget.
  static void tickOne(GuiAnimation& anim, GuiWidget& widget, float dt);
};

}  // namespace eng
