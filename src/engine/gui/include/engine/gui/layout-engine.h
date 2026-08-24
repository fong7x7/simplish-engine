#pragma once

/// @file layout-engine.h
/// @brief Flex-style measure/arrange, scroll state, layout styles for widgets.
/// @par Threading Main thread only.

#include "gui-rect.h"
#include "layout-edges.h"
#include "scroll-state.h"

#include <cstdint>

namespace eng {

// Forward declaration required to break circular dependency:
// gui-widget-tree.h depends on gui-widget.h which depends on layout-engine.h

enum class FlexDirection : uint8_t {
  ROW,
  COLUMN,
};

enum class FlexWrap : uint8_t {
  NO_WRAP,
  WRAP,
};

enum class Align : uint8_t {
  START,
  CENTER,
  END,
  STRETCH,
  SPACE_BETWEEN,
};

enum class PositionMode : uint8_t {
  RELATIVE,
  ABSOLUTE,
};

struct LayoutStyle {
  /// Primary axis direction for flex layout.
  FlexDirection direction = FlexDirection::COLUMN;
  /// Whether flex items wrap to new lines.
  FlexWrap wrap = FlexWrap::NO_WRAP;
  /// Cross-axis alignment for child items.
  Align align_items = Align::STRETCH;
  /// Multi-line cross-axis alignment.
  Align align_content = Align::START;
  /// Main-axis alignment for child items.
  Align justify_content = Align::START;
  /// Override for this item's cross-axis alignment within its parent.
  Align align_self = Align::START;

  /// Flex grow factor for distributing extra space.
  float flex_grow = 0.0f;
  /// Flex shrink factor for overflow reduction.
  float flex_shrink = 1.0f;
  /// Flex basis size (-1 = auto, uses min content).
  float flex_basis = -1.0f;

  /// Inner padding on all four edges.
  Edges padding;
  /// Outer margin on all four edges.
  Edges margin;

  /// Explicit width in pixels (-1 = auto).
  float width = -1.0f;
  /// Explicit height in pixels (-1 = auto).
  float height = -1.0f;
  /// Minimum width constraint in pixels.
  float min_width = 0.0f;
  /// Minimum height constraint in pixels.
  float min_height = 0.0f;
  /// Maximum width constraint in pixels (-1 = unconstrained).
  float max_width = -1.0f;
  /// Maximum height constraint in pixels (-1 = unconstrained).
  float max_height = -1.0f;

  /// Gap between flex items in pixels.
  float gap = 0.0f;

  /// Positioning mode (relative or absolute).
  PositionMode position = PositionMode::RELATIVE;
  /// Absolute X position when position mode is absolute.
  float abs_x = 0.0f;
  /// Absolute Y position when position mode is absolute.
  float abs_y = 0.0f;

  /// Enable horizontal scrolling for this container.
  bool scroll_x = false;
  /// Enable vertical scrolling for this container.
  bool scroll_y = false;
};

/// Tick inertial scroll decay; `dt` is seconds.
void updateScroll(ScrollState& scroll, float dt);

/// Apply scroll delta with clamping to content bounds.
void scrollBy(ScrollState& scroll, float dx, float dy);

}  // namespace eng
