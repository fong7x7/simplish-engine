#pragma once

/// @file editor-build-state.h
/// @brief What the rest of the editor, and an agent, may know of building.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/build/editor-build-kind.h>
#include <editor/build/editor-build-status.h>
#include <filesystem>
#include <string>
#include <vector>

namespace eng::editor {

/// Lines of a finished build's log kept in shell state: enough to see the
/// compiler's complaint, few enough to send to an agent on every read.
inline constexpr size_t EDITOR_BUILD_LOG_TAIL = 40;

/// Lines naming an error kept apart from the tail, so an error scrolled
/// out of it by a long link line is still seen.
inline constexpr size_t EDITOR_BUILD_ERROR_LINES = 20;

/// Lines of what the running playtest's game logic said, kept for reading.
inline constexpr size_t EDITOR_LOGIC_LOG_LINES = 50;

/// The build pipeline's state, mirrored into `EditorShellState` for the
/// agent API and the status line. The job and the loaded library live on
/// the editor; this is only what can be said about them.
/// @thread_safety Main-thread-only.
struct EditorBuildState {
  /// What the last build made, or is making.
  EditorBuildKind kind = EditorBuildKind::LOGIC;
  /// Where it is.
  EditorBuildStatus status = EditorBuildStatus::IDLE;
  /// Builds started this session, so a caller polling for one it started
  /// can tell it from the one before.
  uint32_t builds = 0;
  /// The log the last build wrote.
  std::filesystem::path log{};
  /// The last `EDITOR_BUILD_LOG_TAIL` lines of it, once it has finished.
  std::vector<std::string> log_tail{};
  /// Its lines naming an error, at most `EDITOR_BUILD_ERROR_LINES`.
  std::vector<std::string> errors{};
  /// Whether the open project has game logic of its own: a
  /// `src/CMakeLists.txt`.
  bool has_logic = false;
  /// Whether a built logic library is loaded, and the next playtest runs
  /// it.
  bool logic_loaded = false;
  /// Whether the source has changed since that library was built.
  bool logic_stale = false;
  /// Why the last load of the library failed; empty when it did not.
  std::string logic_error{};
  /// The copy of the library that is loaded; empty with none.
  std::filesystem::path logic_library{};
  /// Where the last deploy that succeeded put the game; empty before one.
  std::filesystem::path deployed{};
};

}  // namespace eng::editor
