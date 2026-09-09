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
  /// What the properties panel is editing.
  GET_SELECTION,
  /// The undo history and its cursor.
  GET_HISTORY,
  /// The level file behind the document: where it is, and whether what is
  /// on screen has been written to it.
  GET_LEVEL,
  /// Every menu command, and whether it would do anything right now.
  LIST_COMMANDS,
  /// Place an asset on a tile, as dragging it from the browser would.
  PLACE_ASSET,
  /// Add a light, as dragging one from the general section would.
  ADD_LIGHT,
  /// Set one property of a placement or a light to an absolute value.
  SET_PROPERTY,
  /// Move a placement or a light by a delta, in tiles.
  TRANSLATE,
  /// Select a placement or a light, or clear the selection.
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
    AgentTool::LIST_LIGHTS,   AgentTool::GET_SELECTION,
    AgentTool::GET_HISTORY,   AgentTool::GET_LEVEL,
    AgentTool::LIST_COMMANDS, AgentTool::PLACE_ASSET,
    AgentTool::ADD_LIGHT,     AgentTool::SET_PROPERTY,
    AgentTool::TRANSLATE,     AgentTool::SELECT,
    AgentTool::SET_TOOL,      AgentTool::RUN_COMMAND,
    AgentTool::UNDO,          AgentTool::REDO,
    AgentTool::OPEN_PROJECT,  AgentTool::RESCAN_ASSETS,
};

}  // namespace eng::editor
