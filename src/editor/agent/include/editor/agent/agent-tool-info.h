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

/// `add_waypoint` adds a point to a patrol route.
inline constexpr AgentParam AGENT_PARAMS_ADD_WAYPOINT[] = {
    {"x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X the waypoint stands at, in tiles. A whole number and a half "
     "is the middle of a tile, which is where a drag from the browser puts "
     "one."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y the waypoint stands at, in tiles."},
    {"z", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Height above the ground plane, in tiles. Defaults to 0; patrols walk "
     "the floor, so it only moves the marker."},
    {"route", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Which route, 1 to 9. Defaults to the selected waypoint's route, or 1 "
     "when no waypoint is selected — so adding several in a row lays one "
     "route out. Out-of-range values are clamped."},
    {"order", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Its place in the route, 1 to 99; an actor walks a route's waypoints "
     "in this order, and in list order where two share one. Defaults to "
     "after the route's last waypoint."},
};

/// `add_emitter` places a particle emitter.
inline constexpr AgentParam AGENT_PARAMS_ADD_EMITTER[] = {
    {"x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X its bursts start from, in tiles. A whole number and a half is "
     "the middle of a tile, which is where a drag from the browser puts "
     "one."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y its bursts start from, in tiles."},
    {"z", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Height above the floor, in tiles. Defaults to 0.9, chest height, where "
     "a drag from the browser puts one."},
    {"effect", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "The preset its burst and flash start from, by id — list_emitters "
     "lists them under effects: muzzle_flash, muzzle_sparks, wall_sparks, "
     "grit, hit_spray, fireball, embers, smoke. Defaults to wall_sparks."},
};

/// `add_sprite` places a sprite billboard.
inline constexpr AgentParam AGENT_PARAMS_ADD_SPRITE[] = {
    {"x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X its base stands at, in tiles. A whole number and a half is "
     "the middle of a tile, which is where a drag from the browser puts "
     "one."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y its base stands at, in tiles."},
    {"z", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Height above the floor, in tiles. Defaults to 0 — feet on the ground, "
     "which is where its depth is measured and where a drag puts one."},
    {"sheet", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "The sprite sheet it shows, by the path list_sprites lists under "
     "sheets — relative to the project's assets directory, as "
     "\"sprites/slime.png\". Defaults to the project's first sheet, and to "
     "none at all when it has no sheets."},
    {"columns", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Frames across the sheet, 1 to 64. Defaults to 1, the whole image as "
     "one frame."},
    {"rows", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Frames down the sheet, 1 to 64. Defaults to 1."},
    {"frames", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "How many of the grid's cells hold a frame, counted left to right and "
     "then down. Defaults to every cell, which is what a sheet whose last "
     "row is full wants; give it when the last row is short, or the sprite "
     "blinks out on the empty cells."},
    {"fps", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Frames a second. Defaults to 12. Zero holds the first frame, which "
     "is what a sheet of facings rather than of animation wants."},
    {"height", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "How tall it stands, in tiles. Defaults to 1, which draws exactly as "
     "tall as a one-tile cube beside it. Its width is not a parameter: it "
     "follows from this and the shape of one frame, so a sheet is never "
     "stretched."},
};

/// `get_ground` reads the ground, or a window of it.
inline constexpr AgentParam AGENT_PARAMS_GET_GROUND[] = {
    {"x", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World X of the window's west column, in tiles. Give x, y, width and "
     "height together, or none of them for the painted part of the ground."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World Y of the window's south row, in tiles."},
    {"width", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Columns in the window, from x east. The window holds at most 65536 "
     "cells."},
    {"height", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Rows in the window, from y north."},
};

/// `paint_ground` fills a rectangle of the ground with one terrain.
inline constexpr AgentParam AGENT_PARAMS_PAINT_GROUND[] = {
    {"terrain", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "The terrain to paint, by word — grass, dirt, sand, stone, road, "
     "hole; get_ground lists them — or \"none\" to erase back to bare "
     "ground. Water is not a terrain: paint_water lays it over these."},
    {"target", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "\"selection\" to repaint the selected area of ground — select it "
     "with select's target \"ground\" — in place of a rectangle, as the "
     "properties panel's Terrain row does. x and y are then not needed."},
    {"x", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World X of the rectangle's west column, in tiles. A fraction names "
     "the tile it falls in. Needed unless target is \"selection\"."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World Y of the rectangle's south row, in tiles."},
    {"width", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Columns to paint, from x east, 1 to 256. Defaults to 1."},
    {"height", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Rows to paint, from y north, 1 to 256. Defaults to 1."},
};

/// `set_sheet` points a billboard at another sheet.
inline constexpr AgentParam AGENT_PARAMS_SET_SHEET[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"sprite\", or \"selection\" when a billboard is selected."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in the sprite list. Not needed when target is "
     "\"selection\"."},
    {"sheet", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "The sheet, by the path list_sprites lists under sheets. The grid, "
     "the speed and the height are kept, so a sheet swapped for one cut "
     "the same way plays straight away."},
};

/// `set_effect` starts an emitter from a preset.
inline constexpr AgentParam AGENT_PARAMS_SET_EFFECT[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"emitter\", or \"selection\" when an emitter is selected."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in the emitter list. Not needed when target is "
     "\"selection\"."},
    {"effect", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "The preset, by id, as list_emitters lists them under effects. Its "
     "burst and flash replace the emitter's; where it stands, which way it "
     "points and how often it bursts are kept."},
};

/// `play_effect` fires an effect once.
inline constexpr AgentParam AGENT_PARAMS_PLAY_EFFECT[] = {
    {"effect", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "What to play: a preset id, as list_emitters lists them under "
     "effects — one burst and its flash — or a whole combat effect: "
     "shot_fired, shot_hit_body, shot_hit_wall or blast. Needed unless "
     "emitter is given."},
    {"emitter", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Index of a placed particle emitter, to fire its own burst once, "
     "where it stands, as it is now — edits and all. Takes the place of "
     "effect, x, y and the rest."},
    {"x", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World X it goes off at, in tiles. Needed with effect."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World Y it goes off at, in tiles. Needed with effect."},
    {"z", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Height above the floor, in tiles. Defaults to 0.9, chest height, "
     "where shots fly — or 0 for a blast, which goes off on the floor."},
    {"dx", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "With dy and dz, which way it points; any length. A preset defaults "
     "to straight up; a shot to +X, and its sparks and spray follow the "
     "shot, so this is the way the shot was travelling. Those not given "
     "are 0 once any is."},
    {"dy", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Y of that direction."},
    {"dz", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Z of that direction."},
    {"scale", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Multiplies every particle's speed and size and the flash's reach; a "
     "blast's radius is 1.5 tiles times it. Defaults to 1."},
};

/// `set_property` writes one number on one entry.
inline constexpr AgentParam AGENT_PARAMS_SET_PROPERTY[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", \"player_start\", \"waypoint\", "
     "\"emitter\", \"sprite\", or "
     "\"selection\" for whatever the properties panel is currently "
     "editing."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Ignored, and not needed, when target is "
     "\"selection\"."},
    {"field", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Property name as `get_selection` reports it: position_x, position_y, "
     "position_z, rotation_x, rotation_y, rotation_z, direction_x, "
     "direction_y, direction_z, color_r, color_g, color_b, intensity, "
     "range, player, collides, scale, route, order, or one of a particle "
     "emitter's: interval, particles, spread, speed_min, speed_max, "
     "life_min, life_max, size_start, size_end, start_r, start_g, start_b, "
     "start_hide, end_r, end_g, end_b, end_hide, gravity, drag, stretch, "
     "flash, flash_range, flash_time, or one of a sprite billboard's: "
     "height, columns, rows, frames, fps. A player start takes "
     "the position and player only, a waypoint the position, route and "
     "order, an emitter its position, direction and its own, and a "
     "billboard its position, height and sheet grid; collides "
     "and scale are a placement's — collides is 1 for "
     "solid and 0 to let players walk through it, and scale is a uniform "
     "size multiplier where 1 is the size the asset was dropped at, "
     "clamped to 0.125 through 8."},
    {"value", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "The value to write. Angles wrap into [-180, 180), colour channels "
     "and direction components are clamped, a player is rounded into 1 to "
     "4, a route into 1 to 9 and an order into 1 to 99, collides is 1 at "
     "0.5 and above; an emitter's particles are rounded into 1 to 200, its "
     "spread held to 0 to 180 degrees, and its lengths, times and colours "
     "kept from going below zero — gravity alone may be negative; a "
     "billboard's columns and rows are rounded into 1 to 64 and its frame "
     "count held to the cells that grid has. The "
     "response reports what was actually stored."},
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
    {"route", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "The patrol route a behavior's patrol state walks, 1 to 9 — its "
     "waypoints are what list_waypoints reports — or 0 for none. Omitted "
     "keeps the prop's route. A patrolling actor with no route stands "
     "where it is."},
    {"footsteps", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "What the actor's feet sound like when it walks in a playtest: "
     "\"default\", \"boots\", \"bare\", \"claws\" or \"heavy\". "
     "Omitted keeps the prop's, which is default until one is chosen."},
};

/// `set_surface` gives a prop the surface a step on it sounds like.
inline constexpr AgentParam AGENT_PARAMS_SET_SURFACE[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", or \"selection\" when a placement is selected."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in the placement list. Not needed when target is "
     "\"selection\"."},
    {"surface", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "ground, grass, dirt, sand, water, stone, wood, metal or cloth; or "
     "\"none\" to take the prop's surface away, so steps on it sound like "
     "the ground under it."},
};

/// `start_playtest` may name who player 1 plays as.
inline constexpr AgentParam AGENT_PARAMS_START_PLAYTEST[] = {
    {"character", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "Who player 1 plays as: a character from list_characters, by id, "
     "reference or name. Omitted, it is the one their start names, or the "
     "project's first character, or the default character when there are "
     "none."},
    {"stand_ins", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Stand-in players to add beside player 1, 0 to 3 — the multi-player "
     "preview: each spawns on its player's start, or beside player 1, and "
     "is played by the game's stand-in, which revives teammates who are "
     "down, backs away from hostiles and keeps up with the others. A "
     "player a pad is seated for is played by that pad instead, and the "
     "playtest adds as many players as there are pads even past this. "
     "Omitted, it is what Level › Play with N Stand-ins last chose, or "
     "none. Remembered for later playtests."},
};

/// `translate` moves an entry by a delta rather than to a position.
inline constexpr AgentParam AGENT_PARAMS_TRANSLATE[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", \"player_start\", \"waypoint\", "
     "\"emitter\", \"sprite\", or "
     "\"selection\"."},
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
     "\"placement\", \"light\", \"player_start\", \"waypoint\", "
     "\"emitter\", \"sprite\", or "
     "\"selection\" for whatever the properties panel is currently "
     "editing."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Ignored, and not needed, when target is "
     "\"selection\". Everything after it moves down one, so delete from "
     "the back when removing several by index."},
};

/// `select` names an entry, or clears the selection.
inline constexpr AgentParam AGENT_PARAMS_SELECT[] = {
    {"target", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "\"placement\", \"light\", \"player_start\", \"waypoint\", "
     "\"emitter\", \"sprite\", \"ground\" or \"water\" for the area of "
     "painted ground or body of water holding a tile, or \"none\"."},
    {"index", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Position in that list. Not needed for \"none\", \"ground\", "
     "\"water\"."},
    {"x", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "With target \"ground\" or \"water\": world X of the tile. Every tile "
     "of its terrain, or of water, joined to it is selected."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "With target \"ground\" or \"water\": world Y of that tile."},
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
     "Whether fire is held. Defaults to false. What firing does is the "
     "project's game logic's: the scaffold's rifle shoots along the aim."},
    {"pause", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "Whether pause is held; false by default. The logic hears a press as "
     "PAUSE_PRESSED; get_playtest's game_paused says if it paused."},
    {"ticks", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Ticks to hold this for, 1 to 3600 (60 is a second). Defaults to 1."},
};

/// `find_path` asks the navigation grid for a route.
inline constexpr AgentParam AGENT_PARAMS_FIND_PATH[] = {
    {"from_x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X the walk starts at, in tiles."},
    {"from_y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y the walk starts at, in tiles."},
    {"to_x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X the walk should end at, in tiles."},
    {"to_y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y the walk should end at, in tiles."},
    {"radius", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "The walker's radius in tiles, 0.05 to 2. Defaults to 0.3, a player's "
     "— and an actor's unless its model is wider."},
};

/// `step_playtest` runs an exact number of ticks.
inline constexpr AgentParam AGENT_PARAMS_STEP_PLAYTEST[] = {
    {"ticks", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Ticks to run, 1 to 3600 (60 is a second). Defaults to 1."},
};

/// `set_water_depth` names a depth and where it goes.
inline constexpr AgentParam AGENT_PARAMS_SET_WATER_DEPTH[] = {
    {"depth", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "puddle (1/16 tile), shallows (1/4), pond (1), lake (3) or deep (8), as "
     "get_water lists them; or tiles from 0.0625 to 15.9, to the nearest "
     "1/16."},
    {"target", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "\"selection\" for the selected area of water, in place of x and y."},
    {"x", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World X of the rectangle's west column, in tiles. Needed unless "
     "target is \"selection\"."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "World Y of the rectangle's south row, in tiles."},
    {"width", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Columns, from x east, 1 to 256. Defaults to 1."},
    {"height", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Rows, from y north, 1 to 256. Defaults to 1."},
};

/// `paint_water` names a rectangle and what to lay over it.
inline constexpr AgentParam AGENT_PARAMS_PAINT_WATER[] = {
    {"x", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World X of the rectangle's west column, in tiles."},
    {"y", AgentParamType::NUMBER, AgentParamNeed::REQUIRED,
     "World Y of the rectangle's south row, in tiles."},
    {"width", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Columns, from x east, 1 to 256. Defaults to 1."},
    {"height", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Rows, from y north, 1 to 256. Defaults to 1."},
    {"depth", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "puddle, shallows, pond, lake or deep, or tiles; defaults to pond."},
    {"color", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "\"#rrggbb\" for water laid where it was dry; defaults to blue-green."},
    {"opacity", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "0 (clear) to 1 (opaque), for water laid where it was dry."},
    {"flow_direction", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Which way water laid where it was dry flows, in degrees "
     "anticlockwise from east. Defaults to 0."},
    {"flow_speed", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "How fast it flows, 0 (standing) to 1 (1.5 tiles a second). Defaults "
     "to 0."},
    {"viscosity", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "How thick it is, 0 (water) to 1 (like honey or mud): thick fluid "
     "carries ripples slower, settles without ringing and barely raises a "
     "wave. Defaults to 0."},
    {"dry", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "true takes the water off instead, as the Dry card does."},
};

/// `set_water_fidelity` names one fidelity.
inline constexpr AgentParam AGENT_PARAMS_SET_WATER_FIDELITY[] = {
    {"fidelity", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "flat — a still surface, nothing simulated; low — a rippling surface "
     "simulated at 4 samples a tile; or high — 8 samples a tile, with wind "
     "waves, light in the shallows and foam on the crests."},
};

/// `set_water_effects` switches any of the water's effects.
inline constexpr AgentParam AGENT_PARAMS_SET_WATER_EFFECTS[] = {
    {"reflections", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "The scene mirrored in the water. Omitted, kept."},
    {"refraction", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "The ground seen through the water bent by its ripples. Omitted, "
     "kept."},
    {"contact", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "Foam rings and shadows at the foot of whatever stands in the water. "
     "Omitted, kept."},
    {"caustics", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "Light the waves focus on the ground under them. Omitted, kept."},
};

/// `set_volume` sets any of the volumes, and the mute.
inline constexpr AgentParam AGENT_PARAMS_SET_VOLUME[] = {
    {"master", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "The volume over everything, 0 to 1 — a slider's position; the gain is "
     "its square. Omitted, kept."},
    {"effects", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "The effects bus — shots, hits, blasts — 0 to 1. Omitted, kept."},
    {"music", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "The music bus, 0 to 1. Omitted, kept."},
    {"interface", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "The interface bus — menus and the HUD — 0 to 1. Omitted, kept."},
    {"muted", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "true silences everything, keeping the volumes; false brings them "
     "back. Omitted, kept."},
};

/// `set_animation_events` gives a clip or a sheet its events.
inline constexpr AgentParam AGENT_PARAMS_SET_ANIMATION_EVENTS[] = {
    {"asset", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "A rigged model — .gltf or .glb — by its reference "
     "(mesh:characters_knight), "
     "id or name, as list_assets lists it. Give it with clip, or give sheet "
     "instead."},
    {"clip", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "One of that model's clips, by name (walk). list_animation_events "
     "lists them once the model has been placed and loaded."},
    {"sheet", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "A sprite sheet, by the path list_sprites lists under sheets "
     "(sprites/slime.png), in place of asset and clip."},
    {"events", AgentParamType::ARRAY, AgentParamNeed::OPTIONAL,
     "A list of events. For a clip, each {\"at\": seconds into the "
     "clip, \"sound\", \"gain\"}; for a sheet, each {\"frame\": frame "
     "number from 0, \"sound\", \"gain\"}. sound is \"footstep\" — the "
     "feet of whoever animates, on the surface under them — or a slot "
     "get_sound lists (combat.blast), or one of get_sound's sound_files "
     "(sounds/swoosh.wav). gain is 0 to 4, default 1. An empty array "
     "silences a clip; omitting events takes the row away, so a clip "
     "steps at its detected foot contacts again."},
};

/// `set_sound` puts a project file in a slot.
inline constexpr AgentParam AGENT_PARAMS_SET_SOUND[] = {
    {"slot", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "The sound, as get_sound lists it — combat.shot_fired, "
     "combat.shot_hit_body, combat.shot_hit_wall or combat.blast, or the "
     "part after the dot."},
    {"file", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "One of get_sound's sound_files, relative to assets/ "
     "(sounds/boom.wav); an empty string plays the built-in sound again."},
};

/// `import_sound` brings a file into the project.
inline constexpr AgentParam AGENT_PARAMS_IMPORT_SOUND[] = {
    {"path", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "The absolute path of a .wav or .ogg file on this machine."},
    {"slot", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "A sound to play it in from now on, as set_sound takes it. Omitted, "
     "the file is only added."},
};

/// `play_sound` plays something once.
inline constexpr AgentParam AGENT_PARAMS_PLAY_SOUND[] = {
    {"name", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "A slot (combat.blast, or blast) as the game plays it now, or one of "
     "get_sound's sound_files."},
};

/// `set_controls` rebinds one action, tunes the deadzones, or resets.
inline constexpr AgentParam AGENT_PARAMS_SET_CONTROLS[] = {
    {"action", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "The action to rebind, as get_controls names it — move_up, fire, "
     "aim_left and so on. Needs controls."},
    {"controls", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "The action's controls, comma-separated, as the bindings file names "
     "them — key:w, key:space, pad:south, pad:-left_y, pad:right_trigger. "
     "They replace all it had; an empty string unbinds it."},
    {"left_stick", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Left stick radial deadzone, 0 to 0.95; omitted, kept."},
    {"right_stick", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Right stick radial deadzone, 0 to 0.95; omitted, kept."},
    {"trigger", AgentParamType::NUMBER, AgentParamNeed::OPTIONAL,
     "Trigger deadzone, 0 to 0.95; omitted, kept."},
    {"reset", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "True first puts every action back on its defaults, keeping the "
     "deadzones, as R on the Controls screen does. Defaults to false."},
};

/// `open_project` points the editor at a directory.
inline constexpr AgentParam AGENT_PARAMS_OPEN_PROJECT[] = {
    {"path", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Directory holding the project. Opening one drops the document held "
     "in memory and reads the new project's own level file, so save first "
     "if the current one has unsaved edits."},
};

inline constexpr AgentParam AGENT_PARAMS_CREATE_PROJECT[] = {
    {"path", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "Directory to create the project in; made if it is not there. Refused "
     "by the editor, which then leaves the open project alone, when it "
     "already holds a project."},
    {"name", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "The project's name. Omitted, it is the directory's own name."},
};

inline constexpr AgentParam AGENT_PARAMS_GET_BUILD[] = {
    {"wait", AgentParamType::BOOLEAN, AgentParamNeed::OPTIONAL,
     "Hold the answer until a running build has finished — the editor keeps "
     "drawing meanwhile — rather than answering running. Defaults to false. "
     "A deploy can take minutes; give the HTTP call a timeout to match."},
};

inline constexpr AgentParam AGENT_PARAMS_GET_PLAYTEST[] = {
    {"until_tick", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Hold the answer until a playtest running in real time reaches this "
     "tick (60 a second), or is paused, stopped or over. Omitted, it "
     "answers at once."},
};

inline constexpr AgentParam AGENT_PARAMS_GET_LOG[] = {
    {"since", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "The first sequence number wanted: pass the last answer's next to hear "
     "only what is new. Defaults to 0, the oldest kept."},
    {"level", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "The least serious level wanted: debug, info, warn or error. Defaults "
     "to info."},
    {"subsystem", AgentParamType::STRING, AgentParamNeed::OPTIONAL,
     "Only lines one subsystem logged — editor, logic, renderer, … Omitted, "
     "every subsystem's."},
    {"limit", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "Most lines to answer with, 1 to 500. Defaults to 100."},
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
inline constexpr AgentParam AGENT_PARAMS_SET_UI_SCREEN[] = {
    {"id", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "The screen's id — lowercase letters, digits, _ and - — which is its "
     "file, content/ui/<id>.ui.json, and what the game logic shows it by "
     "(world.showScreen(\"<id>\"))."},
    {"screen", AgentParamType::OBJECT, AgentParamNeed::REQUIRED,
     "The screen: {\"layer\": \"menu\" (modal, dims the game, takes the pad) "
     "or \"hud\" (over play, takes no input), \"anchor\": center, top, "
     "bottom, left, right, top_left, top_right, bottom_left, bottom_right or "
     "fill (default center), \"inset\": pixels from the edges (24), "
     "\"root\": a node}. A node is {\"type\": panel, label, button, bar or "
     "spacer, \"id\", and for a label or button \"text\" — {key} shows the "
     "value key the logic set — for a button \"action\", for a bar "
     "\"value\" and \"max\" (keys, or a number for max), and for a panel "
     "\"children\": [nodes]}, styled by flexbox: \"direction\" row or "
     "column, \"gap\", \"padding\" and \"margin\" (a number or [top, right, "
     "bottom, left]), \"width\", \"height\", \"min_width\", "
     "\"min_height\", \"grow\", \"align\" and \"align_self\" (start, "
     "center, end, stretch), \"justify\" (start, center, end, "
     "space_between, space_around, space_evenly), \"fill\" and \"color\" "
     "(#rrggbb or #rrggbbaa) and \"radius\". docs/game/ui.md under "
     "toolchain.engine_root has examples."},
};

inline constexpr AgentParam AGENT_PARAMS_RENDER_UI_SCREEN[] = {
    {"id", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "A screen get_ui_screens lists."},
    {"width", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "The view's width in pixels, 16 to 4096; default 1280."},
    {"height", AgentParamType::INTEGER, AgentParamNeed::OPTIONAL,
     "The view's height in pixels, 16 to 4096; default 720."},
    {"values", AgentParamType::OBJECT, AgentParamNeed::OPTIONAL,
     "The values to show, as the logic would set them: {\"score\": 12, "
     "\"health\": 3}. Unset values show as nothing."},
};

inline constexpr AgentParam AGENT_PARAMS_PRESS_UI[] = {
    {"action", AgentParamType::STRING, AgentParamNeed::REQUIRED,
     "An action a button names, as get_ui_screens lists them."},
};

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
    {AgentTool::LIST_WAYPOINTS,
     "list_waypoints",
     "Every waypoint in the level — the points patrol routes are laid out "
     "with — with its index, id, route (1 to 9), order in that route, and "
     "position; and every route in use, with its points in the order an "
     "actor walks them and the ids of the actors that patrol it.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_EMITTERS,
     "list_emitters",
     "Every particle emitter in the level with its index, id, the preset "
     "it was started from and whether it is still exactly that preset, "
     "position, direction, flash tint, and every number of its burst under "
     "the name set_property writes it by; and every preset an emitter can "
     "be started from, by id and name.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::LIST_SPRITES,
     "list_sprites",
     "Every sprite billboard in the level with its index, id, the sheet it "
     "shows, position, and every number the panel lists for it — height, "
     "columns, rows, frames, fps — under the name set_property writes it "
     "by; and every sprite sheet the open project holds, by the path "
     "add_sprite and set_sheet name one with.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_GROUND, "get_ground",
     "The level's painted ground. terrains lists every terrain by number, "
     "word, name and the character rows use for it, bare ground first as "
     "number 0 and \".\"; later terrains are drawn over earlier ones where "
     "they meet. painted is the smallest rectangle holding every painted "
     "cell, window the rectangle rows cover — painted, unless x, y, width "
     "and height name another — and rows one string per row of it, "
     "southmost first, each west to east, one character per cell. Painted "
     "areas are autotiled when drawn: edges and corners round themselves "
     "off from their neighbours, so a road is laid by painting its cells. "
     "water rows the same window the same way, ~ where water lies over the "
     "ground (paint_water) and . where it is dry; the default window takes "
     "the water in too.",
     AgentToolEffect::READ, AGENT_PARAMS_GET_GROUND},
    {AgentTool::GET_EFFECTS,
     "get_effects",
     "What the effects the viewport draws are doing right now, while the "
     "level is edited or played: whose they are (source: editor or "
     "playtest), particles and flashes alive, every emitter with the "
     "bursts it has thrown since it was placed, the level opened, or a "
     "playtest last started or stopped, and how many effects play_effect "
     "has played. Refreshed every frame; to see a burst land, poll it just "
     "after play_effect or while an emitter runs.",
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
    {AgentTool::LIST_ENEMIES,
     "list_enemies",
     "Every enemy archetype the project defines in its enemies data table — "
     "id, name, model, health segments, body radius and height in tiles, "
     "the behavior it runs and whether the project can run it, and its "
     "faction — with the file's path and anything wrong with it. The "
     "archetypes are what the director will spawn a horde from; nothing "
     "spawns them yet. Read when the project opens, on a rescan, and on "
     "every Play; to add or change one, edit that file.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::GET_NAVIGATION,
     "get_navigation",
     "The navigation grid a playtest of the level would plan across — "
     "built from the props that collide, a quarter tile a cell — and what "
     "it says: how many cells are solid, too narrow for an actor a "
     "player's width, walled off from every player start, and open; which "
     "actors no path joins to a player start; and which have no floor "
     "near where they stand. What the View menu's Navigation Overlay "
     "draws.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::FIND_PATH, "find_path",
     "The route an actor of the given radius would plan from one point of "
     "the level to another, by the same A* and smoothing the game runs: "
     "how the search ended (found, unreachable, over_budget, or "
     "blocked_endpoint when an end has no floor near it), the smoothed "
     "waypoints, and the length in tiles. Reads the level as it is now; "
     "nothing changes.",
     AgentToolEffect::READ, AGENT_PARAMS_FIND_PATH},
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
     "onto a tile would, and select it. Recorded as one undoable edit. "
     "It lands solid, except the built-in Tile — the flat shape for "
     "floors, rugs and holes — which lands walkable.",
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
    {AgentTool::ADD_WAYPOINT, "add_waypoint",
     "Add a waypoint to a patrol route, exactly as dragging the waypoint "
     "from the browser's general > tools section would, and select it. An "
     "actor whose behavior has a patrol state walks its route's waypoints "
     "in order, looping or turning back as the state says. Recorded as one "
     "undoable edit, and saved with the level.",
     AgentToolEffect::EDIT, AGENT_PARAMS_ADD_WAYPOINT},
    {AgentTool::ADD_EMITTER, "add_emitter",
     "Add a particle emitter, exactly as dragging the Particle Emitter from "
     "the browser's general > effects section would, and select it. It "
     "throws a burst of particles every interval, and flashes a light over "
     "the meshes near it — in the viewport while the level is edited, and "
     "in a playtest. Start it from a preset, then change any number of its "
     "burst with set_property. Recorded as one undoable edit, and saved "
     "with the level; the simulation never sees it.",
     AgentToolEffect::EDIT, AGENT_PARAMS_ADD_EMITTER},
    {AgentTool::ADD_SPRITE, "add_sprite",
     "Add a sprite billboard, exactly as dragging the Sprite Billboard "
     "from the browser's general > sprites section would, and select it. "
     "It is an upright quad standing at a point in the level, facing the "
     "camera, playing one frame of a sprite sheet at a time; its empty "
     "texels are cut out, so it is hidden by what is in front of it and "
     "hides what is behind it, like any mesh. Recorded as one undoable "
     "edit, and saved with the level; the simulation never sees it, and it "
     "stops nobody.",
     AgentToolEffect::EDIT, AGENT_PARAMS_ADD_SPRITE},
    {AgentTool::PAINT_GROUND, "paint_ground",
     "Paint a rectangle of the ground's cells with one terrain, as the Tile "
     "tool's brush does, or erase it with \"none\". The ground is flat "
     "and drawn under everything placed in the level; where painted cells "
     "meet unpainted ones the edge is rounded, and cells touching only at a "
     "corner join there, so a road or a patch of sand is laid by painting "
     "its cells and nothing else. Reports how many cells changed and the "
     "painted rectangle. Recorded as one undoable edit, and saved with the "
     "level; the simulation does not read the ground yet, so no terrain "
     "stops anybody.",
     AgentToolEffect::EDIT, AGENT_PARAMS_PAINT_GROUND},
    {AgentTool::SET_PROPERTY, "set_property",
     "Set one property of a placement, a light, a player start, a "
     "waypoint or a particle emitter to an absolute value, as typing it "
     "into the properties panel would. An emitter's burst is interval, "
     "particles, spread, speed_min, speed_max, life_min, life_max, "
     "size_start, size_end, start_r/g/b and start_hide (how much of what "
     "is behind a particle it hides: 0 is a pure glow), the same for end_, "
     "gravity (negative rises), drag, stretch (seconds of travel drawn as "
     "a streak), and its flash, flash_range and flash_time. "
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
     "Give a placed prop the behavior it runs in a playtest, the side it "
     "is on, the route it patrols and what its feet sound like, as the "
     "properties panel's Behavior, "
     "Faction, Route and Footsteps rows do. A "
     "prop with a behavior is an actor: when the level is played it sees "
     "and hears the players, plans paths round the level's props, turns "
     "and moves as its behavior's states say, and is no longer a "
     "collision box for players. Recorded as one undoable edit, and saved "
     "with the level.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_BEHAVIOR},
    {AgentTool::SET_SURFACE, "set_surface",
     "Give a placed prop the surface a step on it sounds like — wood for a "
     "deck, cloth for a rug, metal for a grate — as the properties panel's "
     "Surface row does. In a playtest, anybody whose feet are inside the "
     "prop's box, near its height, steps on that surface rather than the "
     "ground's; where two such props overlap, the later in the placement "
     "list wins. Footstep sounds only: it neither stops nor slows anybody. "
     "Recorded as one undoable edit, and saved with the level.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_SURFACE},
    {AgentTool::SET_EFFECT, "set_effect",
     "Start a particle emitter from one of the built-in presets — the "
     "bursts shots, hits and blasts throw — as the properties panel's "
     "Effect row does: its burst and flash become the preset's. Recorded "
     "as one undoable edit.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_EFFECT},
    {AgentTool::SET_SHEET, "set_sheet",
     "Point a sprite billboard at another of the project's sprite sheets, "
     "as the properties panel's Sheet row does. Recorded as one undoable "
     "edit. Refused for a sheet the project does not hold — list_sprites "
     "lists the ones it does.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_SHEET},
    {AgentTool::PLAY_EFFECT, "play_effect",
     "Play an effect once, now, where the viewport shows it: a preset "
     "burst, a whole combat effect (a muzzle flash, a hit, a blast), or a "
     "placed emitter's own burst as it stands — for trying one out without "
     "waiting on an emitter's interval or an actor's volley. Goes into the "
     "editor's effects while the level is edited and the playtest's while "
     "it is played, where a paused playtest holds it until stepped. Not "
     "an edit: nothing is recorded or saved, and it works while playing. "
     "Carried out by the running editor on its next frame; get_effects "
     "shows it land.",
     AgentToolEffect::HOST, AGENT_PARAMS_PLAY_EFFECT},
    {AgentTool::TRANSLATE, "translate",
     "Move a placement, a light, a player start, a waypoint or a particle "
     "emitter by a delta in tiles — "
     "the tool to reach for when asked to shift something in a direction "
     "rather than to a coordinate. Recorded as one undoable edit.",
     AgentToolEffect::EDIT, AGENT_PARAMS_TRANSLATE},
    {AgentTool::DELETE_ENTRY, "delete",
     "Remove a placement, a light, a player start, a waypoint or a particle "
     "emitter from the level — or erase the selected area of ground back to "
     "bare, with target \"selection\" — as the "
     "Delete key does to what is selected. Recorded as one undoable edit, so "
     "undo puts the "
     "entry back where it was; the selection is cleared, and everything "
     "after it in that list is renumbered down one.",
     AgentToolEffect::EDIT, AGENT_PARAMS_DELETE},
    {AgentTool::SELECT, "select",
     "Select a placement, a light, a player start, a waypoint, a particle "
     "emitter, a billboard, or an area of painted ground — every tile of one "
     "terrain joined to the one named — which opens the properties panel "
     "on it, or clear the selection. A selected area is repainted with "
     "paint_ground's target \"selection\", and erased with delete's.",
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
    {AgentTool::GET_PLAYTEST, "get_playtest",
     "Whether the open level is being played, and if so: the tick the "
     "simulation is on, where each player is, who they play as, their "
     "health, whether they are down or out and whether a stand-in or a "
     "seated pad (players 2 to 4) plays them; every actor with its state, "
     "target and health; every "
     "projectile in flight and hazard pool on the floor; the effects "
     "shots, hits and blasts are playing — particles and flashes live now, "
     "and how many shot_fired, shot_hit_body, shot_hit_wall and blast cues "
     "the run has played, and how many of those were sent to be heard "
     "(sounds: the nearest few of each kind a tick); whether the run is "
     "over (no player up, or the game logic ended it) and its outcome "
     "(playing, won or lost); whether the project's game logic runs (logic) "
     "and the last lines it said (logic_log) and sounds and effects it "
     "cued (logic_cues) — actors the logic spawned "
     "are listed after the level's, marked spawned; the "
     "latest tick hash, how many ticks the frame clock has dropped, and "
     "how many ticks of queued input are left. Poll it after "
     "start_playtest or send_input to watch the game run.",
     AgentToolEffect::READ, AGENT_PARAMS_GET_PLAYTEST},
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
    {AgentTool::STEP_PLAYTEST, "step_playtest",
     "Pause the running playtest and run exactly `ticks` ticks of it, on the "
     "input send_input queued — or none — then report it as get_playtest "
     "does. A paused playtest stays where it is until stepped again or "
     "resumed with run_command pause_playtest, so an agent can walk an "
     "actor's decisions tick by tick. Carried out by the running editor on "
     "its next frame; get_playtest shows the result.",
     AgentToolEffect::HOST, AGENT_PARAMS_STEP_PLAYTEST},
    {AgentTool::GET_CONTROLS,
     "get_controls",
     "The user's control scheme — the keys and pad controls the playtest "
     "reads, as Edit › Controls shows them — in the bindings file's own "
     "shape: each action's controls (key:w, pad:south, pad:-left_y), the "
     "pad's deadzones, and the file they are kept in, in the user's "
     "application data rather than the project.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::SET_CONTROLS, "set_controls",
     "Change the user's control scheme, as the Controls screen does: give "
     "one action exactly the controls listed, set the pad's deadzones, or "
     "reset every action to its defaults — any of them in one call, reset "
     "first. Saved to the controls file at once and used by the next tick "
     "of play; not part of the level's undo history. Refused, changing "
     "nothing, when an action or a control is not one there is. Answers "
     "as get_controls does.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_CONTROLS},
    {AgentTool::GET_SOUND,
     "get_sound",
     "The editor's sound, as Edit › Sound shows it: the user's volumes — "
     "master, effects, music, interface, each 0 to 1 — whether it is "
     "muted, and the file they are kept in, in the user's application "
     "data; then each of the game's sounds (slots: combat.shot_fired, "
     "combat.shot_hit_body, combat.shot_hit_wall, combat.blast) and the "
     "project file it plays, null for its built-in sound, and whether that "
     "file is missing; every .wav and .ogg under the project's assets/; the "
     "sounds table's path; and what was wrong with it or the files it "
     "names.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::SET_VOLUME, "set_volume",
     "Set the user's volumes or mute, as the Sound screen does — any of "
     "them in one call. Heard from the next frame and saved to the volumes "
     "file at once; not part of the level's undo history, and allowed while "
     "playing. Refused, changing nothing, on a volume outside 0 to 1. "
     "Answers as get_sound does.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_VOLUME},
    {AgentTool::SET_SOUND, "set_sound",
     "Play one of the project's sound files in one of the game's sounds, "
     "or the built-in sound again, as the Sound screen's left and right "
     "do. Written to content/data/sounds.data.json and heard from the next "
     "sound played; not part of the level's undo history. Refused when no "
     "project is open, the slot names no sound, or the file is not one of "
     "get_sound's sound_files. Answers as get_sound does.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_SOUND},
    {AgentTool::LIST_ANIMATION_EVENTS,
     "list_animation_events",
     "The sounds animations make. clips: every clip of every rigged model "
     "loaded — placed at least once — with its duration, its events (at, "
     "sound, gain) and where they came from: authored, from the project's "
     "content/data/animation-events.data.json; detected, a footstep "
     "wherever a foot joint comes down in the clip; or none. authored_clips "
     "and sheets are the table's rows, models not yet loaded included. In "
     "a playtest a clip's events are heard as its playback reaches them, "
     "and a sheet's as a billboard showing it reaches the frame; a walker "
     "whose clip has footstep events steps with them rather than every "
     "stride.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::SET_ANIMATION_EVENTS, "set_animation_events",
     "Give one clip of a rigged model, or one sprite sheet, the sounds it "
     "makes as it plays, replacing any it had — for a clip, replacing its "
     "detected foot contacts too. Omit events to take the row away. Written "
     "to content/data/animation-events.data.json; not part of the level's "
     "undo history. Answers as list_animation_events does.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_ANIMATION_EVENTS},
    {AgentTool::IMPORT_SOUND, "import_sound",
     "Bring a WAV or Ogg Vorbis file into the open project, as File › "
     "Import Sound does: copied into assets/sounds/ — never over a file "
     "already there; a taken name gets -2, -3 — or used where it is when "
     "it is already under assets/. The file must decode, or it is refused "
     "before anything is copied. With slot, it also plays there from now "
     "on. Answers as get_sound does; the new file is in sound_files.",
     AgentToolEffect::EDIT, AGENT_PARAMS_IMPORT_SOUND},
    {AgentTool::PLAY_SOUND, "play_sound",
     "Play a sound once through the editor's speakers, as Enter on the "
     "Sound screen does: a slot as the game plays it now — the project's "
     "file or the built-in one — or a project sound file. Heard at the "
     "user's volumes; carried out by the running editor on its next frame. "
     "Refused when the name is neither.",
     AgentToolEffect::HOST, AGENT_PARAMS_PLAY_SOUND},
    {AgentTool::GET_WATER,
     "get_water",
     "The level's water, as View › Water draws it: the user's fidelity — "
     "flat, low or high — and every word it can be, and the graphics file "
     "it is kept in, in the user's application data; then what the water "
     "did last frame: drawn, whether a surface was drawn over the ground "
     "(false with no water, or on a backend without a water pipeline); "
     "samples_per_tile, how finely it is shaped — the fidelity's, fewer "
     "when the water is too wide for the budget, 2 at flat, which moves "
     "nothing; wet_samples and water_tiles, how much of it there "
     "is; energy, how much it is moving, 0 when still — drizzle keeps open "
     "water slightly above it; pushes, how many times a step through "
     "it, a shot landing in it or a blast over it has pushed it since the "
     "editor opened; splashes, how many splashes of spray it has thrown up "
     "since then — every fidelity splashes; and obstacles, how many "
     "placements stand in it for its ripples to go round; and effects, "
     "which of reflections, refraction, contact and caustics are drawn. "
     "Everything but fidelity and effects is the frame before the call.",
     AgentToolEffect::READ,
     {}},
    {AgentTool::SET_WATER_FIDELITY, "set_water_fidelity",
     "Draw water flat, low or high, as the View menu's Water rows do. "
     "Drawn from the next frame and saved to the graphics file at once; the "
     "user's setting, not the project's, so not part of the level's undo "
     "history, and allowed while playing. Refused, changing nothing, on a "
     "word that names no fidelity. Answers as get_water does.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_WATER_FIDELITY},
    {AgentTool::SET_WATER_EFFECTS, "set_water_effects",
     "Switch the water's costlier effects on or off, whatever the fidelity, "
     "as the View menu's Water Reflections, Refraction, Contact Foam and "
     "Caustics rows do: each one off is skipped, not merely hidden. Any not "
     "named are left as they are. The user's setting, saved to the graphics "
     "file at once, not part of the level's undo history, and allowed while "
     "playing. Refused, changing nothing, when one is not true or false. "
     "Answers as get_water does, its effects among the rest.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_WATER_EFFECTS},
    {AgentTool::SET_WATER_DEPTH, "set_water_depth",
     "Make water a depth — from a puddle a boot splashes through to a lake "
     "nobody sees the bottom of — in a rectangle of cells or in the "
     "selected body of water, as the properties panel's Depth row does; "
     "only cells of water change, keeping their colour and opacity. Depth "
     "blends across the tile between two depths and shelves to nothing at "
     "every bank: shallow water barely tints the ground under it and stills "
     "fast, deep water hides it and carries ripples further and faster. One "
     "undoable edit; refused while playing. Answers with how many cells "
     "changed and the depth, in tiles and as the panel names it.",
     AgentToolEffect::EDIT, AGENT_PARAMS_SET_WATER_DEPTH},
    {AgentTool::PAINT_WATER, "paint_water",
     "Lay water over a rectangle of the level, as the Water card does — on "
     "top of whatever terrain is there, which stays under it and shows "
     "through as far as the water is clear — or take it off again with dry, "
     "as the Dry card does. Water laid over water changes only its depth, "
     "keeping its colour, opacity and flow; water laid on dry cells takes "
     "the call's colour, opacity and flow — a pond stands, a river runs its "
     "flow_direction at its flow_speed, carrying its ripples and foam. Set "
     "a whole body's afterwards by selecting it (select, target \"water\") "
     "and calling set_property with color_r, color_g, color_b, opacity, "
     "flow_direction, flow_speed or viscosity (a thick fluid moves like "
     "honey). One undoable edit; refused while "
     "playing. Answers with how many cells changed.",
     AgentToolEffect::EDIT, AGENT_PARAMS_PAINT_WATER},
    {AgentTool::GET_BUILD, "get_build",
     "The project's own C++ game logic and the game it deploys to. A "
     "project's rules can be written in C++ in its src/ folder with the "
     "game SDK, <game/sdk/sdk.h>: a class deriving eng::game::sdk::Game, "
     "overriding hooks such as onTick and onActorDied, exported once with "
     "SIMPLISH_GAME_LOGIC(ClassName), every source listed in "
     "src/CMakeLists.txt. docs/game/sdk.md under toolchain.engine_root "
     "documents it; run_command new_game_logic writes a commented example "
     "to start from. Every build is run for ten seconds in a process of "
     "its own before it is loaded; a crash there fails the build, and so do "
     "the project's SIMPLISH_LOGIC_TESTs (listed under TESTS in "
     "src/CMakeLists.txt), run next. run_command "
     "build_game_logic compiles it in "
     "the background, and loads it for the next playtest; run_command "
     "deploy_game bakes every saved level and builds a standalone game "
     "into build/deploy/ (the first deploy builds the engine, and takes "
     "minutes). Call this with wait true to be answered once the build has "
     "finished; build.builds tells your build from the one before. Reports "
     "has_logic, source (the src/ folder), "
     "logic_loaded, logic_stale (the source is newer than the library "
     "loaded: build again), logic_error (why a library would not load), "
     "logic_library, api_version, deployed (the folder of the last deploy "
     "that worked, or null), the toolchain the editor builds with, and "
     "build: kind (logic or deploy), status (idle, running, succeeded, "
     "failed), builds, log (the file holding everything the build "
     "printed), errors (its lines naming an error), diagnostics (the "
     "errors and warnings taken apart: file, line, column, severity, "
     "message), log_tail (its last lines) and tests (each logic test: name, "
     "level, passed, ticks, failures as diagnostics). While playing, "
     "get_playtest "
     "reports logic, outcome and "
     "logic_log: what the logic said with world.log(), and logic_cues: the "
     "sounds and effects it cued with world.cue() (tick, at, sound, effect, "
     "everywhere); a name with no sound or effect is warned of in get_log.",
     AgentToolEffect::READ, AGENT_PARAMS_GET_BUILD},
    {AgentTool::CREATE_PROJECT, "create_project",
     "Create a project in a directory and open it, as File > New Project "
     "does with no dialog: .simplish/project.json, assets/, content/levels/ "
     "and data/. Opening it drops the document held in memory, so save "
     "first. Carried out on the editor's next frame; get_state then shows "
     "it open, or the old project still open when the directory already "
     "held one. run_command new_game_logic then gives it C++ game logic "
     "to start from.",
     AgentToolEffect::HOST, AGENT_PARAMS_CREATE_PROJECT},
    {AgentTool::GET_LOG, "get_log",
     "The editor's recent log, oldest first: every line the engine and the "
     "editor logged, numbered, the last 500 kept. Where the editor says "
     "what it shows nowhere else — a prop dropped because its asset is "
     "gone, a level that will not read, a logic library that will not "
     "load, a table with a bad row — and what the game logic says "
     "(subsystem logic). Each entry has seq, level, subsystem and message; "
     "next is the seq the next line will have, and more says lines were "
     "left out past limit. Call with since set to the last next to hear "
     "only what is new.",
     AgentToolEffect::READ, AGENT_PARAMS_GET_LOG},
    {AgentTool::GET_UI_SCREENS,
     "get_ui_screens",
     "The game's own screens — its menus and HUD, content/ui/<id>.ui.json "
     "(docs/game/ui.md) — built from the engine's GUI widgets: each "
     "screen's id, layer (menu or hud) and buttons (id, action, text); "
     "actions, every action any button names, sorted — what a choice is "
     "numbered by; and problems, each screen file's mistakes by where they "
     "are. The game logic shows a screen with world.showScreen(id), sets "
     "the values its text shows with world.setUiValue(key, text), and "
     "hears a button pressed as a UI_ACTION event (onUiAction).",
     AgentToolEffect::READ,
     {}},
    {AgentTool::SET_UI_SCREEN, "set_ui_screen",
     "Write a game screen to content/ui/<id>.ui.json, replacing any there. "
     "It is checked first: one that is not a screen at all is refused with "
     "its problems; one that reads is written, and any smaller problems "
     "come back in the answer, which is get_ui_screens' after the write. "
     "render_ui_screen shows what it looks like.",
     AgentToolEffect::HOST, AGENT_PARAMS_SET_UI_SCREEN},
    {AgentTool::RENDER_UI_SCREEN, "render_ui_screen",
     "Draw a game screen as the game would show it, over a plain stand-in "
     "for the game, to build/ui/<id>.png in the project — open the image to "
     "see it. Answers with image (its path), width, height, text_drawn "
     "(false when no font was found: laid out, but no letters), buttons "
     "(id, action, text and rect [x, y, w, h] in the image's pixels) and "
     "error.",
     AgentToolEffect::HOST, AGENT_PARAMS_RENDER_UI_SCREEN},
    {AgentTool::PRESS_UI, "press_ui",
     "Choose an action on a game screen as player 1, as a click on its "
     "button would: it is made on the next tick of the playtest, as input, "
     "and the logic hears it the tick after as a UI_ACTION event. Follow "
     "with step_playtest. get_playtest's ui lists the screens shown and "
     "their buttons.",
     AgentToolEffect::EDIT, AGENT_PARAMS_PRESS_UI},
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
