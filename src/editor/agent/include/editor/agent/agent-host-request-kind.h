#pragma once

/// @file agent-host-request-kind.h
/// @brief What a tool still needs the running editor to do.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// The work a tool cannot finish against shell state alone.
///
/// Most of the agent API is a pure function over `EditorShellState`, which
/// is what makes it testable with no window and no GPU. Three things are
/// not: the camera lives in a widget, opening a project touches the disk
/// and the window title, and a rescan destroys and rebuilds GPU textures.
/// Rather than drag the whole editor into the dispatcher, a tool that needs
/// one of those leaves this behind and the editor runs it on the tick that
/// drained the request.
/// @thread_safety Immutable value type.
enum class AgentHostRequestKind : uint8_t {
  /// Nothing further. Every `READ` and `EDIT` tool leaves this.
  NONE,
  /// Run `AgentHostRequest::command` as the menu bar would.
  RUN_COMMAND,
  /// Open the project at `AgentHostRequest::path`.
  OPEN_PROJECT,
  /// Rescan the open project's assets.
  RESCAN_ASSETS,
};

}  // namespace eng::editor
