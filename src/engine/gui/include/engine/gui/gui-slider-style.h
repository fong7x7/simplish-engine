#pragma once

#include "gui-color.h"

#include <optional>

namespace eng {

constexpr float DEFAULT_SLIDER_HANDLE_SIZE = 12.0f;
constexpr float DEFAULT_SLIDER_TRACK_HEIGHT = 4.0f;

/// @thread_safety Main thread only.
struct GuiSliderStyle {
  /// Background track colour; unset takes the theme's `control`.
  std::optional<GuiColor> track_color{};
  /// Filled portion colour (left of handle); unset takes `primary`.
  std::optional<GuiColor> fill_color{};
  /// Draggable handle colour; unset takes `text`.
  std::optional<GuiColor> handle_color{};
  /// Handle colour when hovered or dragging; unset takes `on_primary`.
  std::optional<GuiColor> handle_hover_color{};
  /// Handle width and height in logical pixels.
  float handle_size = DEFAULT_SLIDER_HANDLE_SIZE;
  /// Track bar height in logical pixels.
  float track_height = DEFAULT_SLIDER_TRACK_HEIGHT;
};

}  // namespace eng
