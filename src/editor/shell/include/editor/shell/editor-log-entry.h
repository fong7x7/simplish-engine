#pragma once

/// @file editor-log-entry.h
/// @brief One line of the editor's log, as the agent API reads it.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <engine/core/logger.h>
#include <string>

namespace eng::editor {

/// A line the engine or the editor logged.
/// @thread_safety Immutable value type.
struct EditorLogEntry {
  /// Its place in the session's log, from 0: what `get_log`'s `since` asks
  /// from.
  uint64_t sequence = 0;
  /// How serious it is.
  LogLevel level = LogLevel::INFO;
  /// Who logged it: `editor`, `logic`, `renderer`, …
  std::string subsystem;
  /// What it said.
  std::string message;
};

}  // namespace eng::editor
