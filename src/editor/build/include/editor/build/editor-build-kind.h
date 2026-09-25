#pragma once

/// @file editor-build-kind.h
/// @brief Which of the two things the editor builds a build is.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// What a build makes (ADR-011).
/// @thread_safety Immutable value type.
enum class EditorBuildKind : uint8_t {
  /// The project's game logic, as a shared library a playtest loads.
  LOGIC,
  /// The deployed game: the engine with the project's game logic linked
  /// in, and the project's content beside it.
  DEPLOY,
};

}  // namespace eng::editor
