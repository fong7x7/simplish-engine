#pragma once

/// @file gui-widget-state.h
/// @brief The interaction state a widget is drawn in.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng {

/// The state a widget is drawn in. When several hold, the first in this
/// order wins: DISABLED, PRESSED, SELECTED, HOVER, FOCUSED, NORMAL — so a
/// selected tool stays lit under the pointer, and a disabled one never
/// lights.
enum class GuiWidgetState : uint8_t {
  /// At rest.
  NORMAL,
  /// Under the pointer.
  HOVER,
  /// Held down.
  PRESSED,
  /// Holding keyboard or pad focus (fields show it; buttons show the ring).
  FOCUSED,
  /// Turned off: drawn dimmed and ignoring input.
  DISABLED,
  /// Chosen: the active tool, the open tab.
  SELECTED,
};

/// How many states there are.
inline constexpr std::size_t GUI_WIDGET_STATE_COUNT = 6;

}  // namespace eng
