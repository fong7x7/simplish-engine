#pragma once

/// @file project-open-result.h
/// @brief Outcome of an open or create attempt.
/// @par Threading Main-thread-only.

#include <editor/project/project-context.h>
#include <editor/project/project-open-error.h>

namespace eng::editor {

/// Result of `openProject` / `createProject`. Exactly one of the two states
/// is meaningful: on success `error == ProjectOpenError::NONE` and `context`
/// is loaded; on failure `context` is default-constructed.
///
/// Returned by value rather than thrown — this build has exceptions disabled
/// (docs/decisions/ADR-001-no-exceptions.md).
/// @thread_safety Main-thread-only.
struct ProjectOpenResult {
  /// The opened project. `loaded` is false when `error` is set.
  ProjectContext context;
  /// Why the operation failed, or NONE on success.
  ProjectOpenError error = ProjectOpenError::NONE;

  /// True when a project was opened or created.
  [[nodiscard]] bool ok() const { return error == ProjectOpenError::NONE; }
};

}  // namespace eng::editor
