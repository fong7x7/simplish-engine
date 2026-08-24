#pragma once

/// @file project-context.h
/// @brief The currently open project.
/// @par Threading Main-thread-only.

#include <editor/project/project-metadata.h>
#include <filesystem>

namespace eng::editor {

/// The project the editor currently has open. Default-constructed means
/// "no project loaded" — `loaded` is the only field callers should test.
/// @thread_safety Main-thread-only.
struct ProjectContext {
  /// Absolute path to the project root directory.
  std::filesystem::path root;
  /// Parsed manifest contents.
  ProjectMetadata metadata;
  /// Whether a project is open. False on a default-constructed context.
  bool loaded = false;
};

}  // namespace eng::editor
