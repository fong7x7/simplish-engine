#pragma once

/// @file editor-sound-load.h
/// @brief What loading the project's sounds into a bank did.
/// @par Threading A value type.

#include <game/fx/combat-sounds.h>
#include <string>
#include <vector>

namespace eng::editor {

/// The clips the game's sounds play, after the project's files were loaded
/// over the built-in ones, and what went wrong on the way.
struct EditorSoundLoad {
  /// The clip each combat cue plays, whichever it now is.
  game::CombatSoundClips clips{};
  /// One line for each file that could not be played — missing, or not a
  /// WAV or Ogg Vorbis the decoder reads — or a slot that names nothing;
  /// each of those plays its built-in sound.
  std::vector<std::string> problems{};
};

}  // namespace eng::editor
