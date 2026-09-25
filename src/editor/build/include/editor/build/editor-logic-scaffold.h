#pragma once

/// @file editor-logic-scaffold.h
/// @brief What giving a project game logic of its own did.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// The outcome of `scaffoldProjectLogic`.
/// @thread_safety Immutable value type.
enum class EditorLogicScaffold : uint8_t {
  /// `src/` was written: a build file and an example to start from.
  CREATED,
  /// The project already has game logic; nothing was touched.
  ALREADY_THERE,
  /// It could not be written.
  FAILED,
};

}  // namespace eng::editor
