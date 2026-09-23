#pragma once

/// @file editor-sound-entry.h
/// @brief One row of the project's sounds table: a slot and its file.
/// @par Threading A value type.

#include <filesystem>
#include <string>

namespace eng::editor {

/// A sound the project records in place of a built-in one: which slot —
/// `combat.blast`, the name the clip goes by in the audio bank — and the
/// file under `assets/` it plays instead.
struct EditorSoundEntry {
  /// The slot, as the audio bank names it.
  std::string slot;
  /// The file, relative to the project's `assets/` directory.
  std::filesystem::path file;
};

}  // namespace eng::editor
