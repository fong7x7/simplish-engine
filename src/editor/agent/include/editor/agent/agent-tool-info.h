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

/// `add_player_start` marks where a player spawns.
inline constexpr AgentParam AGENT_PARAMS_ADD_PLAYER_START[] = {
    {"x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X the player's feet land at, in tiles. A whole number and a "
     "half is the middle of a tile, which is where a drag from the browser "
     "puts one."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y the player's feet land at, in tiles."},
    {"z", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Height above the ground plane, in tiles. Defaults to 0, standing on "
     "the ground."},
    {"player", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Which player spawns here, 1 to 4. Defaults to the lowest player with "
     "no start yet, or 1 once all four have one — the same one a drag from "
     "the browser would pick. Out-of-range values are clamped, and the "
     "response reports the player actually stored."},
};

/// `set_property` writes one number on one entry.
inline constexpr AgentParam AGENT_PARAMS_SET_PROPERTY[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", \"player_start\", or \"selection\" for "
     "whatever the properties panel is currently editing."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Ignored, and not needed, when target is "
     "\"selection\"."},
    {"field", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Property name as `get_selection` reports it: position_x, position_y, "
     "position_z, rotation_x, rotation_y, rotation_z, direction_x, "
     "direction_y, direction_z, color_r, color_g, color_b, intensity, "
     "range, player, collides, or scale. A player start takes the position "
     "and player only; collides and scale are a placement's — collides is 1 "
     "for solid and 0 to let players walk through it, and scale is a "
     "uniform size multiplier where 1 is the size the asset was dropped at, "
     "clamped to 0.125 through 8."},
    {"value", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "The value to write. Angles wrap into [-180, 180), colour channels "
     "and direction components are clamped, a player is rounded into 1 to "
     "4, collides is 1 at 0.5 and above, and the response reports what was "
     "actually stored."},
};

/// `set_animation` names the clip a placed rigged model plays.
inline constexpr AgentParam AGENT_PARAMS_SET_ANIMATION[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", or \"selection\" when a placement is selected. Only "
     "placements play clips."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in the placement list. Not needed when target is "
     "\"selection\"."},
    {"clip", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "Name of the clip, one of the `clips` `get_asset` lists for the "
     "placement's asset. Omitted or empty plays the model's first clip, "
     "which is what a placement plays until one is chosen."},
};

/// `set_character` names who a player start's player plays as by default.
inline constexpr AgentParam AGENT_PARAMS_SET_CHARACTER[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"player_start\", or \"selection\" when a player start is selected."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in the player start list. Not needed when target is "
     "\"selection\"."},
    {"character", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "A character from list_characters, by id, reference or name. Omitted "
     "or empty names none, leaving it to the selector."},
};

/// `set_behavior` gives a prop the intelligence it runs in a playtest.
inline constexpr AgentParam AGENT_PARAMS_SET_BEHAVIOR[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", or \"selection\" when a placement is selected."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in the placement list. Not needed when target is "
     "\"selection\"."},
    {"behavior", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "A behavior from list_behaviors, by id, reference or name. Omitted "
     "keeps the prop's behavior; empty takes it away, leaving the prop "
     "scenery again."},
    {"faction", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "\"hostile\", \"neutral\" or \"friendly\". Omitted keeps the "
     "prop's faction, which is hostile until one is chosen."},
};

/// `start_playtest` may name who player 1 plays as.
inline constexpr AgentParam AGENT_PARAMS_START_PLAYTEST[] = {
    {"character", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "Who player 1 plays as: a character from list_characters, by id, "
     "reference or name. Omitted, it is the one their start names, or the "
     "project's first character, or the default character when there are "
     "none."},
};

/// `translate` moves an entry by a delta rather than to a position.
inline constexpr AgentParam AGENT_PARAMS_TRANSLATE[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", \"player_start\", or \"selection\"."},
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

/// `delete` takes an entry back out of the level.
inline constexpr AgentParam AGENT_PARAMS_DELETE[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", \"player_start\", or \"selection\" for "
     "whatever the properties panel is currently editing."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Ignored, and not needed, when target is "
     "\"selection\". Everything after it moves down one, so delete from "
     "the back when removing several by index."},
};

/// `select` names an entry, or clears the selection.
inline constexpr AgentParam AGENT_PARAMS_SELECT[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", \"player_start\", or \"none\" to clear "
     "the selection."},
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

/// `create_level` and `open_level` share what they take, because the
/// second question — what happens to unwritten edits — is the same one.
inline constexpr AgentParam AGENT_PARAMS_CREATE_LEVEL[] = {
    {"id", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Level id: lowercase letters, digits and underscores, starting with a "
     "letter. It becomes the file name and the name generated code uses, "
     "so it is fixed once created."},
    {"unsaved", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "\"refuse\" (the default) to be told no when the open level has "
     "edits that are not in its file, or \"discard\" to throw those edits "
     "away and switch anyway. Save first with run_command and \"save\" to "
     "keep them."},
};

/// `open_level` names a level the project already holds.
inline constexpr AgentParam AGENT_PARAMS_OPEN_LEVEL[] = {
    {"id", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Level id as `list_levels` reports it."},
    {"unsaved", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "\"refuse\" (the default) or \"discard\", as create_level takes."},
};

/// `send_input` queues what player 1 does for a run of ticks.
inline constexpr AgentParam AGENT_PARAMS_SEND_INPUT[] = {
    {"move_x", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Stick along world X, -1 to 1. Defaults to 0."},
    {"move_y", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Stick along world Y, -1 to 1. Defaults to 0."},
    {"aim_x", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Aim X, -1 to 1. An aim of 0, 0 (the default) keeps the last aim."},
    {"aim_y", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Aim along world Y, -1 to 1. Defaults to 0."},
    {"fire", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "Whether fire is held. Defaults to false; nothing fires yet."},
    {"ticks", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Ticks to hold this for, 1 to 3600 (60 is a second). Defaults to 1."},
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
     "camera, what is selected, how many placements, lights and player "
     "starts the level holds, and whether undo and redo have anything to "
     "do.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_ASSETS,
     "list_assets",
     "Every asset scanned from the open project, with its index, name, "
     "path, measured bounds, and state: whether its mesh is loaded, "
     "whether loading it failed, and how far its browser thumbnail got. "
     "A rigged glTF model says `rigged`, and once loaded lists the names "
     "of its animation `clips`.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_ASSET, "get_asset",
     "One asset and its state, by index or by name.", AgentToolEffect::READ,
     AGENT_PARAMS_GET_ASSET},
    {AgentTool::LIST_FOLDERS,
     "list_folders",
     "The folder tree the asset browser shows: the assets root and its "
     "sub-folders, and the built-in general section: its lighting, shapes "
     "and tools subsections, the last holding the player start.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_PLACEMENTS,
     "list_placements",
     "Every asset placed in the level, with its index, the asset it "
     "instances, its position in tiles, its rotation in degrees, "
     "whether players collide with it in a playtest, and the animation "
     "clip it plays — empty for the model's first, and ignored by a model "
     "with no clips.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_LIGHTS,
     "list_lights",
     "Every light in the level, with its index, kind, position, "
     "direction, colour, intensity, and range.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_PLAYER_STARTS,
     "list_player_starts",
     "Every player start in the level — where each player spawns — with "
     "its index, id, the player it is for (1 to 4), its position, and the "
     "character its player plays as by default (empty for none). Also "
     "reports how many players a session holds.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_CHARACTERS,
     "list_characters",
     "Every character the project defines — id, name, model, move speed "
     "in tiles a second, health segments — from the characters data table, "
     "with the file's path and anything wrong with it. The table is read "
     "when the project opens, on a rescan, and on every Play; to add or "
     "change a character, edit that file.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_BEHAVIORS,
     "list_behaviors",
     "Every behavior a prop can run in a playtest — the built-in presets "
     "and the project's own from its behaviors data table, a project row "
     "replacing the preset with its id — each with its id, reference, "
     "name, whether it is built in, its states in order, and the state it "
     "starts in; with the table's path and anything wrong with it. The "
     "table is read when the project opens, on a rescan, and on every "
     "Play; to add or change a behavior, edit that file.",
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
    {AgentTool::LIST_LEVELS,
     "list_levels",
     "Every level the open project holds, by id, with which one is being "
     "edited and whether each has a file on disk yet. A level is a whole "
     "document: opening another replaces the placements, the lights, the "
     "player starts, the selection and the undo history.",
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
    {AgentTool::ADD_PLAYER_START, "add_player_start",
     "Mark where a player spawns, exactly as dragging the player start from "
     "the browser's general > tools section would, and select it. Recorded "
     "as one undoable edit, and saved with the level.",
     AgentToolEffect::EDIT, AGENT_PARAMS_ADD_PLAYER_START},
    {AgentTool::SET_PROPERTY, "set_property",
     "Set one property of a placement, a light or a player start to an "
     "absolute value, as typing it into the properties panel would. "
     "Recorded as one undoable edit, and a write that changes nothing "
     "records nothing.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_PROPERTY},
    {AgentTool::SET_ANIMATION, "set_animation",
     "Choose the animation clip a placed rigged model plays, as the "
     "properties panel's Animation row does. Recorded as one undoable "
     "edit. Refused for a placement whose model has no clips, and for a "
     "clip the model does not have — the error lists the ones it does.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_ANIMATION},
    {AgentTool::SET_CHARACTER, "set_character",
     "Choose who the player who spawns at a player start plays as unless "
     "they pick someone else, as the properties panel's Character row "
     "does: the character the selector opens on. Recorded as one undoable "
     "edit, and saved with the level.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_CHARACTER},
    {AgentTool::SET_BEHAVIOR, "set_behavior",
     "Give a placed prop the behavior it runs in a playtest, and the side "
     "it is on, as the properties panel's Behavior and Faction rows do. A "
     "prop with a behavior is an actor: when the level is played it sees "
     "and hears the players, plans paths round the level's props, turns "
     "and moves as its behavior's states say, and is no longer a "
     "collision box for players. Recorded as one undoable edit, and saved "
     "with the level.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_BEHAVIOR},
    {AgentTool::TRANSLATE, "translate",
     "Move a placement, a light or a player start by a delta in tiles — "
     "the tool to reach for when asked to shift something in a direction "
     "rather than to a coordinate. Recorded as one undoable edit.",
     AgentToolEffect::EDIT, AGENT_PARAMS_TRANSLATE},
    {AgentTool::DELETE_ENTRY, "delete",
     "Remove a placement, a light or a player start from the level, as the "
     "Delete key does to what is selected. Recorded as one undoable edit, so "
     "undo puts the "
     "entry back where it was; the selection is cleared, and everything "
     "after it in that list is renumbered down one.",
     AgentToolEffect::EDIT, AGENT_PARAMS_DELETE},
    {AgentTool::SELECT, "select",
     "Select a placement, a light or a player start, which opens the "
     "properties panel on it, or clear the selection.",
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
    {AgentTool::CREATE_LEVEL, "create_level",
     "Add an empty level to the open project and start editing it, as "
     "Level > New Level does. The level's file is written before the "
     "switch, so it is one list_levels reports even if nothing is placed "
     "in it.",
     AgentToolEffect::HOST, AGENT_PARAMS_CREATE_LEVEL},
    {AgentTool::OPEN_LEVEL, "open_level",
     "Edit another of the open project's levels. Everything the editor "
     "holds belongs to the level being closed — placements, lights, player "
     "starts, selection, undo history — so all of it is replaced by what "
     "the new level's file holds.",
     AgentToolEffect::HOST, AGENT_PARAMS_OPEN_LEVEL},
    {AgentTool::GET_PLAYTEST,
     "get_playtest",
     "Whether the open level is being played, and if so: the tick the "
     "simulation is on, where each player is, who they play as and their "
     "health, "
     "the latest tick hash, how "
     "many ticks the frame clock has dropped, and how many ticks of queued "
     "input are left. Poll it after start_playtest or send_input to watch "
     "the game run.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::START_PLAYTEST, "start_playtest",
     "Play the open level in the real simulation, as the toolbar's Play "
     "button or F5 does — but with no character selector: player 1 plays "
     "as the character named, or the one the selector would open on. "
     "Player 1 spawns at the level's first start for "
     "player 1, or under the camera when it has none. The document is not "
     "changed by playing it, and every document edit is refused until "
     "stop_playtest. The playtest is running by the time this answers, and "
     "advances with the editor's frames — at 60 ticks a second of real "
     "time — whether or not input is sent.",
     AgentToolEffect::HOST, AGENT_PARAMS_START_PLAYTEST},
    {AgentTool::STOP_PLAYTEST,
     "stop_playtest",
     "Stop playing and go back to editing the level exactly as it was. The "
     "run's replay is written to data/playtests/<level>.replay in the "
     "project. While the character selector is up, puts it away instead.",
     AgentToolEffect::HOST,
     {}},
    {AgentTool::SEND_INPUT, "send_input",
     "Queue player 1's input for the next ticks of a running playtest: a "
     "stick, an aim and the fire button, held for a number of ticks. While "
     "any is queued it runs in place of the keyboard, one tick at a time, "
     "behind whatever was queued before it — so a scripted playthrough is "
     "exact and repeatable: the same inputs from the same level give the "
     "same tick hashes. The stick is in world axes, not the camera's: the "
     "keyboard is camera-relative, but a script means the same run under "
     "either projection. Under the isometric view, up the screen is -X and "
     "-Y together. A full stick moves five tiles a second, and a value "
     "outside -1 to 1 is full scale.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SEND_INPUT},
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
