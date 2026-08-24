#pragma once

/// @file gui-easing.h
/// @brief Easing functions for GUI animations.
/// @threading Main-thread only.

#include <cstdint>

namespace eng {

/// Easing curve applied to animation progress.
/// @threading Main-thread only.
enum class GuiEasing : uint8_t {
  LINEAR,
  EASE_IN,
  EASE_OUT,
  EASE_IN_OUT,
};

/// Apply an easing function to normalized time t in [0, 1].
/// Returns eased value in [0, 1].
float applyEasing(GuiEasing easing, float t);

}  // namespace eng
