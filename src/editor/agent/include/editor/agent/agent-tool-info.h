#pragma once

/// @file agent-tool-info.h
/// @brief The published schema for every agent tool.
/// @par Threading Thread-safe (immutable value types).

#include <editor/agent/agent-param.h>
#include <editor/agent/agent-tool-effect.h>
#include <editor/agent/agent-tool.h>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

namespace eng::editor {

/// Where the world axes point, repeated in every tool that takes one.
///
/// The camera has zero yaw (`iso-projection.h`), so the mapping from a
/// world axis to what somebody means by "left" or "further back" is fixed
/// and worth saying in the schema: an agent asked to shift something right
/// has to know that is +X without guessing.
inline constexpr std::string_view AGENT_AXIS_NOTE =
    "World axes: +X runs right across the screen, +Y runs away from the "
    "camera (down-screen), +Z runs straight up. One unit is one tile.";

/// `get_asset` takes whichever way of naming an asset the caller has.
inline constexpr AgentParam AGENT_PARAMS_GET_ASSET[] = {
    {"asset", AgentParamType::ASSET_REF, AgentParamNeed::REQUIRED,
     "Index in the scanned asset list, or the asset's name or path "
     "relative to the assets root."},
};

/// `place_asset` drops a model on a tile.
inline constexpr AgentParam AGENT_PARAMS_PLACE_ASSET[] = {
    {"asset", AgentParamType::ASSET_REF, AgentParamNeed::REQUIRED,
     "Index in the scanned asset list, or the asset's name or path "
     "relative to the assets root."},
    {"x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X of the placement's base, in tiles."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y of the placement's base, in tiles."},
    {"z", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Height above the ground plane, in tiles. Defaults to 0 — standing on "
     "the ground, which is where a drag from the browser puts it."},
};

/// `add_light` drops one of the built-in light sources.
inline constexpr AgentParam AGENT_PARAMS_ADD_LIGHT[] = {
    {"kind", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"directional\" for a sun-like light with parallel rays, or "
     "\"point\" for one that hangs at a position and falls off at its "
     "range."},
    {"x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X the light stands at, in tiles."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y the light stands at, in tiles."},
    {"z", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Height above the ground, in tiles. Defaults to the drop height a "
     "drag from the browser uses, which clears anything on the tile."},
};

/// `set_property` writes one number on one entry.
inline constexpr AgentParam AGENT_PARAMS_SET_PROPERTY[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", or \"selection\" for whatever the "
     "properties panel is currently editing."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Ignored, and not needed, when target is "
     "\"selection\"."},
    {"field", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Property name as `list_placements` and `list_lights` report it: "
     "position_x, position_y, position_z, rotation_x, rotation_y, "
     "rotation_z, direction_x, direction_y, direction_z, color_r, "
     "color_g, color_b, intensity, or range."},
    {"value", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "The value to write. Angles wrap into [-180, 180), colour channels "
     "and direction components are clamped, and the response reports what "
     "was actually stored."},
};

/// `translate` moves an entry by a delta rather than to a position.
inline constexpr AgentParam AGENT_PARAMS_TRANSLATE[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", or \"selection\"."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Not needed when target is \"selection\"."},
    {"dx", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Tiles to move along world X; positive is right. Defaults to 0."},
    {"dy", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Tiles to move along world Y; positive is away from the camera. "
     "Defaults to 0."},
    {"dz", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Tiles to move along world Z; positive is up. Defaults to 0."},
};

/// `select` names an entry, or clears the selection.
inline constexpr AgentParam AGENT_PARAMS_SELECT[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", or \"none\" to clear the selection."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Not needed when target is \"none\"."},
};

/// `set_tool` chooses the toolbar tool.
inline constexpr AgentParam AGENT_PARAMS_SET_TOOL[] = {
    {"tool", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Tool name as `get_state` reports it: select, tile, height, prop, or "
     "entity. Only select does anything today; the rest are inert."},
};

/// `run_command` raises a menu command.
inline constexpr AgentParam AGENT_PARAMS_RUN_COMMAND[] = {
    {"command", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Command name as `list_commands` reports it. A command that command "
     "list marks disabled is refused rather than run."},
};

/// `open_project` points the editor at a directory.
inline constexpr AgentParam AGENT_PARAMS_OPEN_PROJECT[] = {
    {"path", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Directory holding the project. Opening one drops the document held "
     "in memory and reads the new project's own level file, so save first "
     "if the current one has unsaved edits."},
};

/// One tool's published description.
/// @thread_safety Immutable value type.
struct AgentToolInfo {
  /// The tool this row describes.
  AgentTool tool = AgentTool::DESCRIBE;
  /// Name the tool is called by, over HTTP and through MCP.
  std::string_view name;
  /// One or two sentences an agent reads to decide whether this is the
  /// tool it wants. Written for a reader with no other documentation.
  std::string_view summary;
  /// How far running it reaches into the editor.
  AgentToolEffect effect = AgentToolEffect::READ;
  /// What it takes, in the order the schema lists them.
  std::span<const AgentParam> params;
};

/// Every tool's schema, in `AGENT_TOOLS` order.
///
/// This table is the API. `agent-dispatch.cpp` answers exactly these names,
/// the manifest is generated from these rows, and the MCP bridge builds its
/// tool list by reading that manifest — so a tool is described once, here.
inline constexpr AgentToolInfo AGENT_TOOL_INFO[] = {
    {AgentTool::DESCRIBE,
     "describe",
     "What this editor is and every tool it offers, with each tool's "
     "parameters. Call it first when you do not know what the editor can "
     "do.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_STATE,
     "get_state",
     "The editor at a glance: the open project, the active tool, the "
     "camera, what is selected, how many placements and lights the level "
     "holds, and whether undo and redo have anything to do.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_ASSETS,
     "list_assets",
     "Every asset scanned from the open project, with its index, name, "
     "path, measured bounds, and state: whether its mesh is loaded, "
     "whether loading it failed, and how far its browser thumbnail got.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_ASSET, "get_asset",
     "One asset and its state, by index or by name.", AgentToolEffect::READ,
     AGENT_PARAMS_GET_ASSET},
    {AgentTool::LIST_FOLDERS,
     "list_folders",
     "The folder tree the asset browser shows: the assets root and its "
     "sub-folders, and the built-in general section holding the light "
     "sources.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_PLACEMENTS,
     "list_placements",
     "Every asset placed in the level, with its index, the asset it "
     "instances, its position in tiles, and its rotation in degrees.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_LIGHTS,
     "list_lights",
     "Every light in the level, with its index, kind, position, "
     "direction, colour, intensity, and range.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_SELECTION,
     "get_selection",
     "What the properties panel is editing, and the fields it lists for "
     "it.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_HISTORY,
     "get_history",
     "Every edit made this session, oldest first, and how many of them are "
     "currently applied. The ones past that cursor are undone and waiting "
     "to be redone.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_LEVEL,
     "get_level",
     "The level file behind the document: its id, where it is written, "
     "whether one is there yet, whether it could be read, and whether the "
     "document on screen has unwritten changes. Save with run_command and "
     "the command \"save\", which is the same thing File > Save does.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_COMMANDS,
     "list_commands",
     "Every menu command, its label, its keyboard shortcut, and whether it "
     "would do anything right now. A disabled command names a feature the "
     "editor does not have yet.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::PLACE_ASSET, "place_asset",
     "Place an asset in the level, exactly as dragging it from the browser "
     "onto a tile would, and select it. Recorded as one undoable edit.",
     AgentToolEffect::EDIT, AGENT_PARAMS_PLACE_ASSET},
    {AgentTool::ADD_LIGHT, "add_light",
     "Add a light to the level, exactly as dragging one from the general "
     "section would, and select it. Recorded as one undoable edit.",
     AgentToolEffect::EDIT, AGENT_PARAMS_ADD_LIGHT},
    {AgentTool::SET_PROPERTY, "set_property",
     "Set one property of a placement or a light to an absolute value, as "
     "typing it into the properties panel would. Recorded as one undoable "
     "edit, and a write that changes nothing records nothing.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_PROPERTY},
    {AgentTool::TRANSLATE, "translate",
     "Move a placement or a light by a delta in tiles — the tool to reach "
     "for when asked to shift something in a direction rather than to a "
     "coordinate. Recorded as one undoable edit.",
     AgentToolEffect::EDIT, AGENT_PARAMS_TRANSLATE},
    {AgentTool::SELECT, "select",
     "Select a placement or a light, which opens the properties panel on "
     "it, or clear the selection.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SELECT},
    {AgentTool::SET_TOOL, "set_tool", "Choose the active toolbar tool.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_TOOL},
    {AgentTool::RUN_COMMAND, "run_command",
     "Run a menu command — the camera and grid commands, undo and redo, "
     "closing the project, quitting. Anything the menu bar can raise.",
     AgentToolEffect::HOST, AGENT_PARAMS_RUN_COMMAND},
    {AgentTool::UNDO,
     "undo",
     "Revert the newest edit, and move the selection with it. Reports "
     "unavailable when the history has nothing applied.",
     AgentToolEffect::EDIT,
     {}},
    {AgentTool::REDO,
     "redo",
     "Reapply the newest reverted edit. Reports unavailable when nothing "
     "has been undone.",
     AgentToolEffect::EDIT,
     {}},
    {AgentTool::OPEN_PROJECT, "open_project",
     "Open the project in a directory, as File > Open Project would.",
     AgentToolEffect::HOST, AGENT_PARAMS_OPEN_PROJECT},
    {AgentTool::RESCAN_ASSETS,
     "rescan_assets",
     "Rescan the open project's assets from disk. This drops the level "
     "and its undo history, because a rescan renumbers the asset list "
     "every placement and every action names.",
     AgentToolEffect::HOST,
     {}},
};

static_assert(std::size(AGENT_TOOL_INFO) == std::size(AGENT_TOOLS),
              "every agent tool needs a published schema row");

/// The schema row for @p tool.
[[nodiscard]] constexpr const AgentToolInfo& agentToolInfo(AgentTool tool) {
  for (const AgentToolInfo& info : AGENT_TOOL_INFO) {
    if (info.tool == tool) {
      return info;
    }
  }
  return AGENT_TOOL_INFO[0];
}

/// The name @p tool is called by.
[[nodiscard]] constexpr std::string_view agentToolName(AgentTool tool) {
  return agentToolInfo(tool).name;
}

/// How far running @p tool reaches into the editor.
[[nodiscard]] constexpr AgentToolEffect agentToolEffect(AgentTool tool) {
  return agentToolInfo(tool).effect;
}

/// The tool called @p name, or nothing when no tool is.
///
/// Nothing falls back to a default here: a caller that misspells a tool
/// gets told it did, rather than getting whichever tool happens to be
/// first.
[[nodiscard]] constexpr std::optional<AgentTool>
findAgentTool(std::string_view name) {
  for (const AgentToolInfo& info : AGENT_TOOL_INFO) {
    if (info.name == name) {
      return info.tool;
    }
  }
  return std::nullopt;
}

}  // namespace eng::editor
