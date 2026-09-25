#pragma once

/// @file editor-build-diagnostic.h
/// @brief One complaint a build made, where it made it.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <editor/build/editor-diagnostic-severity.h>
#include <string>

namespace eng::editor {

/// An error or warning out of a build's log, taken apart: what an agent
/// fixing a build needs without reading compiler output.
/// @thread_safety Immutable value type.
struct EditorBuildDiagnostic {
  /// The file it is about, as the tool named it; empty when it named none
  /// — a link failure, a crash in the logic check.
  std::string file;
  /// The line, from 1; 0 when there is none.
  uint32_t line = 0;
  /// The column, from 1; 0 when there is none.
  uint32_t column = 0;
  /// Error or warning.
  EditorDiagnosticSeverity severity = EditorDiagnosticSeverity::ERROR;
  /// What it said.
  std::string message;
};

}  // namespace eng::editor
