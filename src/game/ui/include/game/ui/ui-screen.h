#pragma once

/// @file ui-screen.h
/// @brief One of a game's screens: a menu or a HUD.
/// @par Threading
/// A value type.

#include <game/ui/ui-anchor.h>
#include <game/ui/ui-node.h>
#include <game/ui/ui-screen-layer.h>
#include <string>

namespace eng::game {

/// A screen, as `content/ui/<id>.ui.json` describes it (docs/game/ui.md).
struct UiScreen {
  /// What the game logic shows it by: the file's name.
  std::string id{};
  /// A menu or a HUD.
  UiScreenLayer layer = UiScreenLayer::MENU;
  /// Where its root sits on the view.
  UiAnchor anchor = UiAnchor::CENTER;
  /// How far in from the view's edges its root is kept, in pixels.
  float inset = 24.0F;
  /// Its root node.
  UiNode root{};
};

}  // namespace eng::game
