#pragma once

/// @file editor-build-status.h
/// @brief How the editor's last build went.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// Where a build is.
/// @thread_safety Immutable value type.
enum class EditorBuildStatus : uint8_t {
  /// Nothing has been built this session.
  IDLE,
  /// A build is running in the background.
  RUNNING,
  /// The last build finished, and made what it was meant to.
  SUCCEEDED,
  /// The last build failed: a command failed, or its output was not
  /// where it should be. Its log says why.
  FAILED,
};

}  // namespace eng::editor
