#pragma once

/// @file editor-log-state.h
/// @brief The editor's recent log, mirrored into shell state.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-log-entry.h>
#include <vector>

namespace eng::editor {

/// Lines of the log kept for reading: enough for a session's warnings, few
/// enough that nothing grows without bound.
inline constexpr size_t EDITOR_LOG_LINES = 500;

/// The last `EDITOR_LOG_LINES` lines logged, oldest first — what the
/// agent API's `get_log` reads. Everything the editor warns of but shows
/// nowhere else: a prop dropped because its asset is gone, a level that
/// will not read, a logic library that will not load.
/// @thread_safety Main-thread-only.
struct EditorLogState {
  /// The lines kept, oldest first.
  std::vector<EditorLogEntry> entries;
  /// The sequence number the next line logged will have.
  uint64_t next = 0;
};

}  // namespace eng::editor
