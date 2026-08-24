#pragma once

#include "gui-color.h"

namespace eng {

constexpr float DEFAULT_SLIDER_HANDLE_SIZE = 12.0f;
constexpr float DEFAULT_SLIDER_TRACK_HEIGHT = 4.0f;

/// @thread_safety Main thread only.
struct GuiSliderStyle {
  /// Background track color.
  GuiColor track_color{};
  /// Filled portion color (left of handle).
  GuiColor fill_color{};
  /// Draggable handle color.
  GuiColor handle_color{};
  /// Handle color when hovered or dragging.
  GuiColor handle_hover_color{};
  /// Handle width and height in logical pixels.
  float handle_size = DEFAULT_SLIDER_HANDLE_SIZE;
  /// Track bar height in logical pixels.
  float track_height = DEFAULT_SLIDER_TRACK_HEIGHT;
};

}  // namespace eng
