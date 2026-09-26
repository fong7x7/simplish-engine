#pragma once

/// @file layout-auto-margins.h
/// @brief Which of a widget's margins are `auto`.
/// @par Threading
/// Plain data.

namespace eng {

/// Which margins are CSS's `auto`: along the main axis they share out any
/// room left after growing — `left` alone pushes an item to the far end
/// of its row, both sides centre it — and across it they align the item
/// in its line in place of `align_self`.
struct LayoutAutoMargins {
  /// Top margin is auto.
  bool top = false;
  /// Right margin is auto.
  bool right = false;
  /// Bottom margin is auto.
  bool bottom = false;
  /// Left margin is auto.
  bool left = false;
};

}  // namespace eng
