#pragma once

/// @file editor-playtest-button.h
/// @brief A button of a game screen shown in the playtest.
/// @par Threading
/// A value type.

#include <engine/gui/gui-rect.h>
#include <string>

namespace eng::editor {

/// One button on a screen the game logic shows: what an agent reads to
/// know what it can press, and where it is.
struct EditorPlaytestButton {
  /// The screen it is on.
  std::string screen{};
  /// Its node's id; empty when it has none.
  std::string id{};
  /// The action it chooses.
  std::string action{};
  /// Its text, values filled in.
  std::string text{};
  /// Where it is in the editor's window, in pixels, as last laid out.
  Rect rect{};
};

}  // namespace eng::editor
