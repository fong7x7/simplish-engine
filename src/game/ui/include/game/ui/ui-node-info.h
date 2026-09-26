#pragma once

/// @file ui-node-info.h
/// @brief One named node of a built screen, as an agent or a test sees it.
/// @par Threading
/// A value type.

#include <engine/gui/gui-rect.h>
#include <game/ui/ui-node-kind.h>
#include <string>

namespace eng::game {

/// A node with an `id` on a built screen: what it is, where it was laid
/// out, and the flags its bindings set.
struct UiNodeInfo {
  /// Its id.
  std::string id{};
  /// What it is.
  UiNodeKind kind = UiNodeKind::PANEL;
  /// Where it is, in the view's pixels, as last laid out.
  Rect rect{};
  /// Whether it is shown; a hidden node takes no room.
  bool visible = true;
  /// Whether it is dimmed and not pressable.
  bool disabled = false;
  /// Whether it is drawn chosen.
  bool selected = false;
};

}  // namespace eng::game
