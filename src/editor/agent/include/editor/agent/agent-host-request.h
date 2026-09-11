#pragma once

/// @file agent-host-request.h
/// @brief What the editor must carry out after a tool has run.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/agent/agent-host-request-kind.h>
#include <editor/shell/editor-level-unsaved.h>
#include <editor/shell/editor-menu-command.h>
#include <string>

namespace eng::editor {

/// A piece of work handed back to the running editor.
///
/// A value rather than a callback: the dispatcher never holds a pointer to
/// the editor, so the whole tool surface stays testable against a bare
/// `EditorShellState`, and what a tool asks the editor for is something a
/// test can assert on rather than something it has to observe happening.
/// @thread_safety Main-thread-only.
struct AgentHostRequest {
  /// What the editor has to do, or `NONE`.
  AgentHostRequestKind kind = AgentHostRequestKind::NONE;
  /// The command to run, meaningful only for `RUN_COMMAND`.
  EditorMenuCommand command = EditorMenuCommand::SEPARATOR;
  /// The project directory to open, meaningful only for `OPEN_PROJECT`.
  std::string path;
  /// The level id to create or open, meaningful only for `CREATE_LEVEL`
  /// and `OPEN_LEVEL`.
  std::string level;
  /// What those two do about edits the open level has not written. The
  /// tool has already refused the call when this is `REFUSE` and there are
  /// any, so by the time the editor reads it the answer is settled.
  EditorLevelUnsaved unsaved = EditorLevelUnsaved::REFUSE;
  /// The character id player 1 plays as, meaningful only for
  /// `START_PLAYTEST`; empty for the default character.
  std::string character{};
  /// Ticks to run, meaningful only for `STEP_PLAYTEST`.
  uint32_t ticks = 0;
};

}  // namespace eng::editor
