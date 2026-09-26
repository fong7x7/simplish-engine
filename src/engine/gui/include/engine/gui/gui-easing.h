#pragma once

/// @file gui-easing.h
/// @brief Easing functions for GUI animations.
/// @threading Main-thread only.

#include <cstdint>

namespace eng {

/// Easing curve applied to animation progress.
/// @threading Main-thread only.
enum class GuiEasing : uint8_t {
  /// Constant speed.
  LINEAR,
  /// Cubic: slow start, fast end.
  EASE_IN,
  /// Cubic: fast start, slow end. The default for a state change.
  EASE_OUT,
  /// Smoothstep: slow at both ends.
  EASE_IN_OUT,
  /// CSS's `ease`, `GUI_BEZIER_EASE`.
  EASE,
  /// Most of the move at once, then a soft arrival — for things entering:
  /// `GUI_BEZIER_EMPHASIZED`.
  EMPHASIZED,
  /// Past the end and back: `GUI_BEZIER_BACK_OUT`.
  BACK_OUT,
  /// A damped spring, `GUI_SPRING_DEFAULT`, over the animation's duration:
  /// a quick move with a little bounce.
  SPRING,
};

/// Apply an easing function to normalized time t in [0, 1]. Returns the
/// eased progress: 0 at 0 and 1 at 1, past 1 on the way for the curves
/// that overshoot (BACK_OUT, SPRING).
float applyEasing(GuiEasing easing, float t);

}  // namespace eng
