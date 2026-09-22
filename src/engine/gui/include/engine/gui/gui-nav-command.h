#pragma once

/// @file gui-nav-command.h
/// @brief One step of focus navigation: a direction, confirm, cancel, or
///        a move through focus order.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// What a player asks a menu to do without a pointer — a d-pad press, a
/// face button, a shoulder. Device-neutral: `GuiGamepadNavigator` makes
/// these from a pad, and anything else (a keyboard, an agent, a test) can
/// make them too. `GuiWidgetTree::routeNav` carries them out.
enum class GuiNavCommand : uint8_t {
  UP,        ///< Focus the nearest widget above, or step a widget up
  DOWN,      ///< Focus the nearest widget below, or step a widget down
  LEFT,      ///< Focus the nearest widget to the left, or step it left
  RIGHT,     ///< Focus the nearest widget to the right, or step it right
  CONFIRM,   ///< Activate the focused widget: press, pick, start typing
  CANCEL,    ///< Back out: stop typing, or let the menu close
  NEXT,      ///< The next widget in focus order, wrapping
  PREVIOUS,  ///< The previous widget in focus order, wrapping
};

}  // namespace eng
