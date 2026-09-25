#pragma once

/// @file editor-logic-library-load.h
/// @brief Loading a project's game logic library, and why a load failed.
/// @par Threading Main-thread-only.

#include <editor/build/editor-logic-library.h>
#include <filesystem>
#include <memory>
#include <string>

namespace eng::editor {

/// A load that worked, or the reason it did not.
/// @thread_safety Main-thread-only.
struct EditorLogicLibraryLoad {
  /// The library, open; null when the load failed.
  std::shared_ptr<EditorLogicLibrary> library;
  /// Why it failed, for the status line and the agent; empty on success.
  std::string error;
};

/// Load the library at @p built: copied to @p load_path first, then
/// opened, checked for the three exports and for the API version these
/// headers describe. Nothing, and why, when any of that fails.
[[nodiscard]] EditorLogicLibraryLoad
loadEditorLogicLibrary(const std::filesystem::path& built,
                       const std::filesystem::path& load_path);

}  // namespace eng::editor
