#pragma once

/// @file ui-button-info.h
/// @brief One button of a built screen, as an agent or a test sees it.
/// @par Threading
/// A value type.

#include <engine/gui/gui-rect.h>
#include <string>

namespace eng::game {

/// A button on a built screen: which, what it chooses, what it says, and
/// where it was laid out.
struct UiButtonInfo {
  /// Its node's id; empty when it has none.
  std::string id{};
  /// The action it chooses.
  std::string action{};
  /// Its text, values filled in.
  std::string text{};
  /// Where it is, in the view's pixels, as last laid out.
  Rect rect{};
};

}  // namespace eng::game
