#pragma once

/// @file editor-dialog-purpose.h
/// @brief What the editor asked the OS file dialog for.
/// @par Threading Main-thread-only.

#include <cstdint>

namespace eng::editor {

/// Why a save-location dialog is open.
///
/// The platform client offers one save dialog and answers it through one
/// `onSaveLocationChosen`, so what to do with the path it returns has to be
/// remembered from the command that asked. Two commands ask, and both want
/// a name typed into the same dialog.
/// @thread_safety Main-thread-only.
enum class EditorDialogPurpose : uint8_t {
  /// The path names a directory to create a project in.
  NEW_PROJECT,
  /// The name typed becomes a level id in the open project.
  NEW_LEVEL,
};

}  // namespace eng::editor
