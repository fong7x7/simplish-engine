#pragma once

/// @file gui-focus-change.h
/// @brief Whether a widget just took focus or lost it.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// What happened to a widget's focus, for `GuiWidget::handleFocusChange`.
enum class GuiFocusChange : uint8_t {
  /// It has focus now and did not.
  GAINED,
  /// It had focus and does not.
  LOST,
};

}  // namespace eng
