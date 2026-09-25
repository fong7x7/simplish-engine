#pragma once

/// @file layout-engine.h
/// @brief The style a widget is laid out by: CSS flexbox, in logical pixels.
/// `GuiWidgetTree::computeLayout` measures every widget bottom-up and then
/// places each container's children by it; `technical/layout-engine.md`
/// has the algorithm.
/// @par Threading Main thread only.

#include "gui-rect.h"
#include "layout-edges.h"
#include "scroll-state.h"

#include <cstdint>

namespace eng {

/// The axis a container lays its children along — its main axis.
enum class FlexDirection : uint8_t {
  /// Left to right.
  ROW,
  /// Top to bottom.
  COLUMN,
};

/// Whether children that overflow the main axis start a new line.
enum class FlexWrap : uint8_t {
  /// One line, shrinking children to fit.
  NO_WRAP,
  /// As many lines as it takes.
  WRAP,
};

/// Where free space goes — along the main axis for `justify_content`,
/// across it for `align_items`, `align_self` and `align_content`.
enum class Align : uint8_t {
  /// Packed at the start.
  START,
  /// Centred.
  CENTER,
  /// Packed at the end.
  END,
  /// Filling the line (cross axis only; as START along the main axis).
  STRETCH,
  /// First and last at the edges, the rest spread evenly between.
  SPACE_BETWEEN,
  /// Equal space round each: half as much at the edges as between.
  SPACE_AROUND,
  /// Equal space between each and at the edges.
  SPACE_EVENLY,
  /// `align_self` only: take the parent's `align_items`.
  AUTO,
};

/// Whether a widget takes part in its parent's flow.
enum class PositionMode : uint8_t {
  /// Placed in flow by the parent's flex layout.
  RELATIVE,
  /// Out of flow, placed by the `abs_*` insets from the parent's edges.
  ABSOLUTE,
  /// Out of flow and never placed: its rect is set by hand, or by another
  /// widget — a menu bar placing its dropdowns below itself, say. The
  /// layout leaves it, and its children, alone.
  MANUAL,
};

/// How a widget is sized and placed. Sizes are border-box: `width` and
/// `height` include the padding, never the margin.
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
  /// Override for this item's cross-axis alignment within its parent;
  /// AUTO uses the parent's `align_items`.
  Align align_self = Align::AUTO;

  /// Flex grow factor for distributing extra space.
  float flex_grow = 0.0f;
  /// Flex shrink factor for overflow reduction.
  float flex_shrink = 1.0f;
  /// Main-axis size before growing or shrinking (-1 = auto: the explicit
  /// size, else the measured one).
  float flex_basis = -1.0f;

  /// Space inside the edges, round the children or content.
  Edges padding;
  /// Space outside the edges, kept clear of siblings and the parent's
  /// content edge. Adds to `gap`; margins never collapse.
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

  /// Space between adjacent children, and between wrapped lines.
  float gap = 0.0f;

  /// Positioning mode (relative or absolute).
  PositionMode position = PositionMode::RELATIVE;
  /// ABSOLUTE: distance from the parent's left edge to the left margin.
  float abs_x = 0.0f;
  /// ABSOLUTE: distance from the parent's top edge to the top margin.
  float abs_y = 0.0f;
  /// ABSOLUTE: distance from the parent's right edge to the right margin
  /// (-1 = unset). With an auto `width` it stretches the widget from
  /// `abs_x` to here; with an explicit one it anchors the right edge.
  float abs_right = -1.0f;
  /// ABSOLUTE: distance from the parent's bottom edge to the bottom margin
  /// (-1 = unset), as `abs_right` is to the right.
  float abs_bottom = -1.0f;

  /// Enable horizontal scrolling for this container. Not read by the
  /// layout: a scrolling list is a `GuiScrollPanel`.
  bool scroll_x = false;
  /// Enable vertical scrolling for this container.
  bool scroll_y = false;
};

/// Tick inertial scroll decay; `dt` is seconds.
void updateScroll(ScrollState& scroll, float dt);

/// Apply scroll delta with clamping to content bounds.
void scrollBy(ScrollState& scroll, float dx, float dy);

}  // namespace eng
