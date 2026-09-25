#pragma once

/// @file ui-render-size.h
/// @brief The view a screen is rendered at, in pixels.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The largest side a screen is rendered at.
inline constexpr uint32_t UI_RENDER_MAX_SIDE = 4096;

/// How big a view to lay a screen out in and draw.
struct UiRenderSize {
  /// Width, in pixels.
  uint32_t width = 1280;
  /// Height, in pixels.
  uint32_t height = 720;
};

}  // namespace eng::game
