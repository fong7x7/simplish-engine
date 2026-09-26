#pragma once

/// @file gui-motion.h
/// @brief Whether the interface moves: the reduced-motion preference.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng {

/// How much the interface animates — CSS's `prefers-reduced-motion`.
/// REDUCED finishes every animation the frame it starts: state blends,
/// glides, and things entering and leaving land at once, and callbacks
/// that wait on them still run. Timers (a toast's life, a tooltip's delay)
/// keep time as before.
enum class GuiMotion : uint8_t {
  /// Animate.
  FULL,
  /// Land at once.
  REDUCED,
};

/// How far an animation steps in a frame of @p dt seconds under
/// @p motion: @p dt, or far enough to finish any animation.
[[nodiscard]] constexpr float guiMotionStep(GuiMotion motion, float dt) {
  return motion == GuiMotion::REDUCED ? 1.0e6f : dt;
}

}  // namespace eng
