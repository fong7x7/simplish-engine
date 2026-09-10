#pragma once

/// @file agent-tool.h
/// @brief Every tool the editor's agent API offers.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// One callable operation on the editor, as an agent sees it.
///
/// The whole agent surface is this enum. A tool that exists here has a
/// schema row in `agent-tool-info.h` and a dispatch arm in
/// `agent-dispatch.cpp`, and the switches over it carry no `default`, so a
/// tool added here fails the build until both of those grow with it. That
/// is the mechanism behind the rule in
/// [docs/editor/agent-api.md](../../../../../docs/editor/agent-api.md):
/// nothing reaches an agent by being remembered, only by being enumerated.
///
/// The MCP bridge builds its own tool list from this one at run time, so a
/// tool added here needs no edit on the bridge side at all.
/// @thread_safety Immutable value type.
enum class AgentTool : uint8_t {
  /// What this editor is, and every tool it offers.
  DESCRIBE,
  /// Project, active tool, camera, selection, and what the document holds.
  GET_STATE,
  /// Every asset the open project was scanned for, and how far each got.
  LIST_ASSETS,
  /// One asset, by index or by name.
  GET_ASSET,
  /// The folder tree the asset browser navigates.
  LIST_FOLDERS,
  /// Every asset placed in the level.
  LIST_PLACEMENTS,
  /// Every light in the level.
  LIST_LIGHTS,
  /// Every player start in the level.
  LIST_PLAYER_STARTS,
  /// What the properties panel is editing.
  GET_SELECTION,
  /// The undo history and its cursor.
  GET_HISTORY,
  /// The level file behind the document: where it is, and whether what is
  /// on screen has been written to it.
  GET_LEVEL,
  /// Every level the open project holds, and which one is being edited.
  LIST_LEVELS,
  /// Every menu command, and whether it would do anything right now.
  LIST_COMMANDS,
  /// Place an asset on a tile, as dragging it from the browser would.
  PLACE_ASSET,
  /// Add a light, as dragging one from the general section would.
  ADD_LIGHT,
  /// Add a player start, as dragging one from general > tools would.
  ADD_PLAYER_START,
  /// Set one property of a placement, a light or a player start to an
  /// absolute value.
  SET_PROPERTY,
  /// Move a placement, a light or a player start by a delta, in tiles.
  TRANSLATE,
  /// Take a placement, a light or a player start back out of the level.
  ///
  /// Not `DELETE`: `<windows.h>` defines that as an access mask, and a
  /// macro cannot be scoped away by an enum class. The wire name is
  /// `delete`, which is what an agent actually calls it.
  DELETE_ENTRY,
  /// Select a placement, a light or a player start, or clear the selection.
  SELECT,
  /// Choose the active toolbar tool.
  SET_TOOL,
  /// Run a menu command by name.
  RUN_COMMAND,
  /// Revert the newest edit.
  UNDO,
  /// Reapply the newest reverted edit.
  REDO,
  /// Open the project in a directory.
  OPEN_PROJECT,
  /// Rescan the open project's assets from disk.
  RESCAN_ASSETS,
  /// Add a level to the open project and edit it.
  CREATE_LEVEL,
  /// Edit another of the open project's levels.
  OPEN_LEVEL,
  /// Whether the level is being played, and where the players are.
  GET_PLAYTEST,
  /// Start playing the open level, as the toolbar's Play button does.
  START_PLAYTEST,
  /// Stop playing, going back to the level as it was.
  STOP_PLAYTEST,
  /// Queue player 1's input for the next ticks of a running playtest.
  SEND_INPUT,
};

/// Every tool, in the order the manifest lists them.
///
/// The reads come first and the writes after, which is the order somebody
/// reading the manifest wants them in: what can be asked, then what can be
/// changed.
inline constexpr AgentTool AGENT_TOOLS[] = {
    AgentTool::DESCRIBE,      AgentTool::GET_STATE,
    AgentTool::LIST_ASSETS,   AgentTool::GET_ASSET,
    AgentTool::LIST_FOLDERS,  AgentTool::LIST_PLACEMENTS,
    AgentTool::LIST_LIGHTS,   AgentTool::LIST_PLAYER_STARTS,
    AgentTool::GET_SELECTION, AgentTool::GET_HISTORY,
    AgentTool::GET_LEVEL,     AgentTool::LIST_LEVELS,
    AgentTool::LIST_COMMANDS, AgentTool::PLACE_ASSET,
    AgentTool::ADD_LIGHT,     AgentTool::ADD_PLAYER_START,
    AgentTool::SET_PROPERTY,  AgentTool::TRANSLATE,
    AgentTool::DELETE_ENTRY,  AgentTool::SELECT,
    AgentTool::SET_TOOL,      AgentTool::RUN_COMMAND,
    AgentTool::UNDO,          AgentTool::REDO,
    AgentTool::OPEN_PROJECT,  AgentTool::RESCAN_ASSETS,
    AgentTool::CREATE_LEVEL,  AgentTool::OPEN_LEVEL,
    AgentTool::GET_PLAYTEST,  AgentTool::START_PLAYTEST,
    AgentTool::STOP_PLAYTEST, AgentTool::SEND_INPUT,
};

}  // namespace eng::editor
