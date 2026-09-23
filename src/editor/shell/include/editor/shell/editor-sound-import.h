#pragma once

/// @file editor-sound-import.h
/// @brief Bringing a sound file into a project.
/// @par Threading Main-thread-only (reads and copies files).

#include <editor/shell/editor-sound-import-result.h>
#include <filesystem>
#include <string_view>

namespace eng::editor {

/// The folder under `assets/` imported sounds are copied into.
inline constexpr std::string_view EDITOR_IMPORTED_SOUNDS_DIR = "sounds";

/// Bring the sound at @p source into the project whose assets are under
/// @p assets_dir, and say where it is now, relative to that directory.
///
/// The file must decode — a WAV or Ogg Vorbis the engine can play — or it
/// is refused before anything is copied. One already under @p assets_dir
/// is used where it is. Anything else is copied into `sounds/`, under its
/// own name, or with `-2`, `-3` … added when that name is taken: an import
/// never overwrites a file the project has.
[[nodiscard]] EditorSoundImport
importEditorSound(const std::filesystem::path& assets_dir,
                  const std::filesystem::path& source);

}  // namespace eng::editor
