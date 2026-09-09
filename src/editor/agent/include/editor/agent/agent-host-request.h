#pragma once

/// @file agent-host-request.h
/// @brief What the editor must carry out after a tool has run.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-host-request-kind.h>
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
};

}  // namespace eng::editor
