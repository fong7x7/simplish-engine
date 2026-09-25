#pragma once

/// @file editor-diagnostic-severity.h
/// @brief Whether a compiler's complaint stops the build.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// How bad a build diagnostic is.
/// @thread_safety Immutable value type.
enum class EditorDiagnosticSeverity : uint8_t {
  /// The build fails on it.
  ERROR,
  /// Worth fixing; the engine's flags make most of these errors too.
  WARNING,
};

}  // namespace eng::editor
