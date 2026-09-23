#pragma once

/// @file editor-sound-import-result.h
/// @brief Where an imported sound went, or why it did not.
/// @par Threading A value type.

#include <filesystem>
#include <string>

namespace eng::editor {

/// What `importEditorSound` did: the file's place under `assets/`, or a
/// sentence saying why there is none.
struct EditorSoundImport {
  /// Where the sound is, relative to `assets/`; empty when it was refused.
  std::filesystem::path file;
  /// Why it was refused; empty when it was not.
  std::string error;
};

}  // namespace eng::editor
