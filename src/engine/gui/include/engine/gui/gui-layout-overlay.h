#pragma once

/// @file gui-layout-overlay.h
/// @brief Whether the tree draws its layout over itself, as a browser's
/// inspector does.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng {

/// What `GuiWidgetTree::renderAll` draws over everything else.
enum class GuiLayoutOverlay : uint8_t {
  /// Nothing.
  OFF,
  /// Every widget's border box as a hairline, and the widget under the
  /// pointer's whole box model — margin, padding, content — tinted as a
  /// browser's inspector tints them, with its name and size.
  BOXES,
};

}  // namespace eng
