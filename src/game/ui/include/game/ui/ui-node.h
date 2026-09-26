#pragma once

/// @file ui-node.h
/// @brief One node of a game screen, and its children.
/// @par Threading
/// A value type.

#include <game/ui/ui-bindings.h>
#include <game/ui/ui-node-kind.h>
#include <game/ui/ui-node-style.h>
#include <string>
#include <vector>

namespace eng::game {

/// A widget a screen file describes: what it is, what it shows, what it
/// does, how it is laid out, and what it holds.
struct UiNode {
  /// What it is.
  UiNodeKind kind = UiNodeKind::PANEL;
  /// Its name, for agents and tests to find it by; empty for none.
  std::string id{};
  /// A label's, a button's, a checkbox's or a toggle's text; `{key}` shows the
  /// value `key`.
  std::string text{};
  /// A button's, checkbox's or toggle's action: what pressing it chooses.
  std::string action{};
  /// A bar's value, by key.
  std::string value{};
  /// A bar's full value, by key; a number when the text is one.
  std::string max{};
  /// Its layout and colours.
  UiNodeStyle style{};
  /// Its flags that follow values.
  UiBindings bind{};
  /// A panel's children, in order.
  std::vector<UiNode> children{};
};

}  // namespace eng::game
