#pragma once

/// @file gui-animation.h
/// @brief Animation definition structs for GUI widget property interpolation.
/// @threading Main-thread only.

#include "gui-color.h"
#include "gui-easing.h"

#include <cstdint>
#include <functional>

namespace eng {

/// Default animation duration in seconds.
constexpr float DEFAULT_ANIM_DURATION = 0.3f;

/// Identifies which widget property an animation targets.
/// @threading Main-thread only.
enum class GuiAnimProperty : uint8_t {
  RECT_X,
  RECT_Y,
  RECT_W,
  RECT_H,
  OPACITY,
  FILL_COLOR,
  BORDER_COLOR,
};

/// A single property animation running on a widget.
/// @threading Main-thread only.
struct GuiAnimation {
  /// Holds the start or target value for an animation (scalar or color).
  /// @threading Main-thread only.
  struct Value {
    /// Scalar value for float properties (rect, opacity, radius).
    float scalar = 0.0f;
    /// Color value for color properties (fill, border).
    GuiColor color{};
  };
  /// Which property to animate.
  GuiAnimProperty property = GuiAnimProperty::OPACITY;
  /// Value at the start of the animation.
  Value start{};
  /// Value at the end of the animation.
  Value target{};
  /// Total duration in seconds.
  float duration = DEFAULT_ANIM_DURATION;
  /// Elapsed time in seconds.
  float elapsed = 0.0f;
  /// Easing curve to apply.
  GuiEasing easing = GuiEasing::EASE_OUT;
  /// Optional callback fired when the animation completes.
  std::function<void()> on_complete{};
};

}  // namespace eng
