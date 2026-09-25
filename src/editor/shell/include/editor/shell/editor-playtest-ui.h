#pragma once

/// @file editor-playtest-ui.h
/// @brief The game's own screens as the playtest shows them.
/// @par Threading
/// A value type.

#include <editor/shell/editor-playtest-button.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace eng::editor {

/// What the game logic shows of its screens (ADR-012), mirrored for agents
/// and the status line, and the one thing that flows back: a choice an
/// agent made, waiting for the next tick.
struct EditorPlaytestUi {
  /// The screens shown, by id, the top one last.
  std::vector<std::string> open{};
  /// Every value the logic has set, by key.
  std::map<std::string, std::string, std::less<>> values{};
  /// Every button on the screens shown, top screen's last.
  std::vector<EditorPlaytestButton> buttons{};
  /// An action `press_ui` chose, made on the next tick; empty for none.
  std::string press{};
};

}  // namespace eng::editor
