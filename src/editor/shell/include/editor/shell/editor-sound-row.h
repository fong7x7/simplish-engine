#pragma once

/// @file editor-sound-row.h
/// @brief One row of the Sound screen, as it is shown.
/// @par Threading A value type.

#include <editor/shell/editor-sound-row-kind.h>
#include <string>

namespace eng::editor {

/// What one row of the Sound screen shows. The editor builds these from its
/// state and the widget only draws them, so every change goes through the
/// editor and is saved the same way an agent's is.
struct EditorSoundRow {
  /// How it is drawn and what the keys do on it.
  EditorSoundRowKind kind = EditorSoundRowKind::HEADING;
  /// What names it: "Master", "Blast", "Project sounds".
  std::string name;
  /// Which setting it is: `master` or a bus's name for a volume, the slot
  /// (`combat.blast`) for a sound; empty for a heading or the mute.
  std::string key;
  /// What it is set to, as text: "80%", "On", "sounds/boom.wav".
  std::string value;
  /// A volume's level, 0 to 1, for its bar; zero for any other row.
  float level = 0.0f;
};

}  // namespace eng::editor
