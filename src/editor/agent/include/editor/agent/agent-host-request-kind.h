#pragma once

/// @file agent-host-request-kind.h
/// @brief What a tool still needs the running editor to do.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// The work a tool cannot finish against shell state alone.
///
/// Most of the agent API is a pure function over `EditorShellState`, which
/// is what makes it testable with no window and no GPU. A few things are
/// not: the camera lives in a widget, opening a project touches the disk
/// and the window title, a rescan destroys and rebuilds GPU textures, and
/// switching level uploads the new level's meshes and re-reads the panels
/// from a document that has been replaced wholesale, and the effects the
/// viewport draws live in the editor, not in its state.
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
  /// Create a project at `AgentHostRequest::path`, named
  /// `AgentHostRequest::name`, and open it.
  CREATE_PROJECT,
  /// Rescan the open project's assets.
  RESCAN_ASSETS,
  /// Create the level `AgentHostRequest::level` and edit it.
  CREATE_LEVEL,
  /// Edit the level `AgentHostRequest::level`.
  OPEN_LEVEL,
  /// Play the open level as `AgentHostRequest::character`, with no selector.
  START_PLAYTEST,
  /// Pause the running playtest and run `AgentHostRequest::ticks` ticks.
  STEP_PLAYTEST,
  /// Play `AgentHostRequest::effect` once, into the viewport's effects.
  PLAY_EFFECT,
  /// Play `AgentHostRequest::sound` once.
  PLAY_SOUND,
  /// Write the game screen `AgentHostRequest::name` as
  /// `AgentHostRequest::text`, and read the screens again.
  WRITE_UI_SCREEN,
  /// Render the game screen `AgentHostRequest::name` at
  /// `AgentHostRequest::width` by `height`, showing the values
  /// `AgentHostRequest::text` holds as a JSON object, to a PNG.
  RENDER_UI_SCREEN,
  /// Write the game screens' theme as `AgentHostRequest::text`, and read
  /// the screens again.
  WRITE_UI_THEME,
  /// Describe the editor's widgets `AgentHostRequest::widgets` asks for.
  DESCRIBE_WIDGETS,
};

}  // namespace eng::editor
