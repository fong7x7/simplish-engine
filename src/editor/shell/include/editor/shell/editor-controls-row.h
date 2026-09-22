#pragma once

/// @file editor-controls-row.h
/// @brief One action's line on the Controls screen.
/// @par Threading Thread-safe (value type).

#include <engine/input/input-action.h>
#include <string>

namespace eng::editor {

/// What the Controls screen shows for one action: its name and what is
/// bound to it on each device, already labelled.
struct EditorControlsRow {
  /// The action the row rebinds.
  input::InputAction action = input::InputAction::MOVE_UP;
  /// Its name: "Move up".
  std::string name;
  /// Its keys: "W / Up".
  std::string keys;
  /// Its pad controls, labelled for the pad in use: "Left Stick Up / D-pad Up".
  std::string pad;
};

}  // namespace eng::editor
