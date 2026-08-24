#pragma once

/// @file project-open-error.h
/// @brief Failure modes for opening or creating a project.
/// @par Threading Thread-safe (immutable enum + pure function).

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// Why a project could not be opened or created.
/// @thread_safety Immutable value type.
enum class ProjectOpenError : uint8_t {
  /// No error — the operation succeeded.
  NONE,
  /// The given path does not exist.
  PATH_NOT_FOUND,
  /// The given path exists but is not a directory.
  NOT_A_DIRECTORY,
  /// The directory exists but holds no `.simplish/project.json`.
  NOT_A_PROJECT,
  /// The manifest exists but could not be read from disk.
  UNREADABLE,
  /// The manifest was read but is not valid JSON.
  MALFORMED,
  /// A project already exists at the target path (create only).
  ALREADY_EXISTS,
  /// The project directory or manifest could not be written (create only).
  WRITE_FAILED,
};

/// Human-readable message for an error code. Returns an empty view for NONE.
[[nodiscard]] constexpr std::string_view
projectOpenErrorMessage(ProjectOpenError error) {
  switch (error) {
    case ProjectOpenError::NONE:
      return {};
    case ProjectOpenError::PATH_NOT_FOUND:
      return "Path does not exist";
    case ProjectOpenError::NOT_A_DIRECTORY:
      return "Path is not a directory";
    case ProjectOpenError::NOT_A_PROJECT:
      return "Directory is not a Simplish project";
    case ProjectOpenError::UNREADABLE:
      return "Project file could not be read";
    case ProjectOpenError::MALFORMED:
      return "Project file is not valid JSON";
    case ProjectOpenError::ALREADY_EXISTS:
      return "A project already exists at this path";
    case ProjectOpenError::WRITE_FAILED:
      return "Project files could not be written";
  }
  return "Unknown error";
}

}  // namespace eng::editor
