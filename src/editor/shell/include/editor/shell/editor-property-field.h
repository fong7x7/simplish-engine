#pragma once

/// @file editor-property-field.h
/// @brief The properties the panel lists, one row each.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>

namespace eng::editor {

/// One editable number on whatever the editor has selected.
///
/// The panel is a list of these rather than a hand-written form per kind of
/// selection: the rows, the hit testing, and the edits all run off this
/// enum, so a light's intensity is another entry here and another arm in
/// `editor-property-ops.cpp` rather than a second panel.
///
/// One enum covers placements, lights, player starts, waypoints and particle
/// emitters together, so a field two of them share — a position — is one row
/// definition and one edit path.
/// The three members of every triple are declared in component order, which is
/// what lets `editor-property-ops.cpp` take a component index by subtraction
/// rather than a case per axis.
/// @thread_safety Immutable value type.
enum class EditorPropertyField : uint8_t {
  /// World X, in tiles.
  POSITION_X,
  /// World Y, in tiles.
  POSITION_Y,
  /// Height above the ground plane, in tiles.
  POSITION_Z,
  /// Rotation about world X, in degrees.
  ROTATION_X,
  /// Rotation about world Y, in degrees.
  ROTATION_Y,
  /// Rotation about world Z, in degrees.
  ROTATION_Z,
  /// X of the direction a light arrives from.
  DIRECTION_X,
  /// Y of the direction a light arrives from.
  DIRECTION_Y,
  /// Z of the direction a light arrives from.
  DIRECTION_Z,
  /// Red channel of a light's tint.
  COLOR_R,
  /// Green channel of a light's tint.
  COLOR_G,
  /// Blue channel of a light's tint.
  COLOR_B,
  /// Brightness multiplier of a light.
  INTENSITY,
  /// How far a point light reaches, in tiles.
  RANGE,
  /// Which player a start is for, 1 to `EDITOR_PLAYER_SLOTS`.
  PLAYER,
  /// Whether a placed prop blocks players: 1 for solid, 0 for not.
  COLLIDES,
  /// Uniform size multiplier of a placed prop; 1 is the one-tile fit.
  SCALE,
  /// Which route a waypoint belongs to, 1 to `EDITOR_ROUTE_COUNT`.
  ROUTE,
  /// Where a waypoint comes in its route, 1 to `EDITOR_WAYPOINT_MAX_ORDER`.
  ORDER,
  /// Seconds between a particle emitter's bursts.
  EMIT_INTERVAL,
  /// Particles an emitter throws in each burst.
  PARTICLES,
  /// Half-angle of the cone a burst is thrown in, in degrees.
  SPREAD,
  /// Slowest a particle leaves, in tiles per second.
  SPEED_MIN,
  /// Fastest a particle leaves, in tiles per second.
  SPEED_MAX,
  /// Shortest a particle lives, in seconds.
  LIFE_MIN,
  /// Longest a particle lives, in seconds.
  LIFE_MAX,
  /// A particle's radius at birth, in tiles.
  SIZE_START,
  /// A particle's radius when it dies, in tiles.
  SIZE_END,
  /// Red light a particle adds at birth.
  START_R,
  /// Green light a particle adds at birth.
  START_G,
  /// Blue light a particle adds at birth.
  START_B,
  /// How much of what is behind it a particle hides at birth.
  START_HIDE,
  /// Red light a particle adds when it dies.
  END_R,
  /// Green light a particle adds when it dies.
  END_G,
  /// Blue light a particle adds when it dies.
  END_B,
  /// How much of what is behind it a particle hides when it dies.
  END_HIDE,
  /// How hard particles fall, in tiles per second squared; negative rises.
  GRAVITY,
  /// How quickly particles slow down.
  DRAG,
  /// Seconds of its travel a particle is drawn stretched along.
  STRETCH,
  /// Brightness of the flash each burst lights the scene with.
  FLASH,
  /// How far that flash reaches, in tiles.
  FLASH_RANGE,
  /// How long that flash lasts, in seconds.
  FLASH_TIME,
  /// How tall a sprite billboard stands, in tiles.
  HEIGHT,
  /// Frames across a sprite sheet.
  COLUMNS,
  /// Frames down a sprite sheet.
  ROWS,
  /// How many of a sheet's cells hold a frame.
  FRAMES,
  /// How many frames a second a sheet plays.
  FPS,
  /// How fast a particle turns, in degrees a second.
  SPIN,
  /// Whether particles are puffs broken up by noise rather than discs.
  TEXTURED,
  /// Whether the scene's lights reach the particles.
  LIT,
  /// How much of the ground under it a body of water hides, 0 to 1.
  OPACITY,
  /// Which way a body of water flows, in degrees anticlockwise from east.
  FLOW_DIRECTION,
  /// How fast a body of water flows, 0 for standing to 1 for
  /// `WATER_MAX_FLOW_SPEED`.
  FLOW_SPEED,
};

/// Every field there is, in the enum's own order.
///
/// The traits table beside it is indexed by this order and asserts against
/// this length, so a field added to the enum and forgotten here fails the
/// build rather than reading another field's label.
inline constexpr EditorPropertyField EDITOR_ALL_PROPERTY_FIELDS[] = {
    EditorPropertyField::POSITION_X,  EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z,  EditorPropertyField::ROTATION_X,
    EditorPropertyField::ROTATION_Y,  EditorPropertyField::ROTATION_Z,
    EditorPropertyField::DIRECTION_X, EditorPropertyField::DIRECTION_Y,
    EditorPropertyField::DIRECTION_Z, EditorPropertyField::COLOR_R,
    EditorPropertyField::COLOR_G,     EditorPropertyField::COLOR_B,
    EditorPropertyField::INTENSITY,   EditorPropertyField::RANGE,
    EditorPropertyField::PLAYER,      EditorPropertyField::COLLIDES,
    EditorPropertyField::SCALE,       EditorPropertyField::ROUTE,
    EditorPropertyField::ORDER,       EditorPropertyField::EMIT_INTERVAL,
    EditorPropertyField::PARTICLES,   EditorPropertyField::SPREAD,
    EditorPropertyField::SPEED_MIN,   EditorPropertyField::SPEED_MAX,
    EditorPropertyField::LIFE_MIN,    EditorPropertyField::LIFE_MAX,
    EditorPropertyField::SIZE_START,  EditorPropertyField::SIZE_END,
    EditorPropertyField::START_R,     EditorPropertyField::START_G,
    EditorPropertyField::START_B,     EditorPropertyField::START_HIDE,
    EditorPropertyField::END_R,       EditorPropertyField::END_G,
    EditorPropertyField::END_B,       EditorPropertyField::END_HIDE,
    EditorPropertyField::GRAVITY,     EditorPropertyField::DRAG,
    EditorPropertyField::STRETCH,     EditorPropertyField::FLASH,
    EditorPropertyField::FLASH_RANGE, EditorPropertyField::FLASH_TIME,
    EditorPropertyField::HEIGHT,      EditorPropertyField::COLUMNS,
    EditorPropertyField::ROWS,        EditorPropertyField::FRAMES,
    EditorPropertyField::FPS,         EditorPropertyField::SPIN,
    EditorPropertyField::TEXTURED,    EditorPropertyField::LIT,
    EditorPropertyField::OPACITY,     EditorPropertyField::FLOW_DIRECTION,
    EditorPropertyField::FLOW_SPEED,
};

/// What the panel lists for a placed asset, in the order it lists them.
inline constexpr EditorPropertyField EDITOR_PLACEMENT_FIELDS[] = {
    EditorPropertyField::POSITION_X, EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z, EditorPropertyField::ROTATION_X,
    EditorPropertyField::ROTATION_Y, EditorPropertyField::ROTATION_Z,
    EditorPropertyField::SCALE,      EditorPropertyField::COLLIDES,
};

/// What the panel lists for a directional light.
///
/// No position: a light with parallel rays shades a scene the same wherever
/// it stands, and a row that changes nothing but a marker is a row that
/// lies about what it does.
inline constexpr EditorPropertyField EDITOR_DIRECTIONAL_LIGHT_FIELDS[] = {
    EditorPropertyField::DIRECTION_X, EditorPropertyField::DIRECTION_Y,
    EditorPropertyField::DIRECTION_Z, EditorPropertyField::INTENSITY,
    EditorPropertyField::COLOR_R,     EditorPropertyField::COLOR_G,
    EditorPropertyField::COLOR_B,
};

/// What the panel lists for a point light: where it is and how far it
/// carries, in place of the direction it has none of.
inline constexpr EditorPropertyField EDITOR_POINT_LIGHT_FIELDS[] = {
    EditorPropertyField::POSITION_X, EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z, EditorPropertyField::RANGE,
    EditorPropertyField::INTENSITY,  EditorPropertyField::COLOR_R,
    EditorPropertyField::COLOR_G,    EditorPropertyField::COLOR_B,
};

/// What the panel lists for a player start: which player, then where.
///
/// The player first, because it is the one number about a start that
/// somebody reaching for the panel is most likely to be changing; the
/// position they have usually just set by where they dropped it.
inline constexpr EditorPropertyField EDITOR_PLAYER_START_FIELDS[] = {
    EditorPropertyField::PLAYER,
    EditorPropertyField::POSITION_X,
    EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z,
};

/// What the panel lists for a waypoint: its route and its place in it,
/// then where it stands.
inline constexpr EditorPropertyField EDITOR_WAYPOINT_FIELDS[] = {
    EditorPropertyField::ROUTE,      EditorPropertyField::ORDER,
    EditorPropertyField::POSITION_X, EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z,
};

/// What the panel lists for a particle emitter: where it stands and which
/// way it throws, how often, then its burst from the cone out — speed, life,
/// size, colour, how it moves — and last the flash each burst lights.
///
/// Long enough that the panel scrolls; the Effect row that starts it from a
/// preset sits above all of these, so it is the one row always in reach.
inline constexpr EditorPropertyField EDITOR_EMITTER_FIELDS[] = {
    EditorPropertyField::POSITION_X,    EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z,    EditorPropertyField::DIRECTION_X,
    EditorPropertyField::DIRECTION_Y,   EditorPropertyField::DIRECTION_Z,
    EditorPropertyField::EMIT_INTERVAL, EditorPropertyField::PARTICLES,
    EditorPropertyField::SPREAD,        EditorPropertyField::SPEED_MIN,
    EditorPropertyField::SPEED_MAX,     EditorPropertyField::LIFE_MIN,
    EditorPropertyField::LIFE_MAX,      EditorPropertyField::SIZE_START,
    EditorPropertyField::SIZE_END,      EditorPropertyField::START_R,
    EditorPropertyField::START_G,       EditorPropertyField::START_B,
    EditorPropertyField::START_HIDE,    EditorPropertyField::END_R,
    EditorPropertyField::END_G,         EditorPropertyField::END_B,
    EditorPropertyField::END_HIDE,      EditorPropertyField::GRAVITY,
    EditorPropertyField::DRAG,          EditorPropertyField::STRETCH,
    EditorPropertyField::SPIN,          EditorPropertyField::TEXTURED,
    EditorPropertyField::LIT,           EditorPropertyField::FLASH,
    EditorPropertyField::FLASH_RANGE,   EditorPropertyField::FLASH_TIME,
};

/// What the panel lists for a sprite billboard: where it stands and how
/// tall, then how its sheet is cut and how fast it runs.
///
/// No width: a frame is whatever shape it was drawn, and the width that
/// keeps that shape is derived from the height
/// (`editor-sprite-transform.h`). A row that could stretch a sprite is a
/// row that will. The Sheet row that picks the image itself leads the list,
/// above all of these.
inline constexpr EditorPropertyField EDITOR_SPRITE_FIELDS[] = {
    EditorPropertyField::POSITION_X, EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z, EditorPropertyField::HEIGHT,
    EditorPropertyField::COLUMNS,    EditorPropertyField::ROWS,
    EditorPropertyField::FRAMES,     EditorPropertyField::FPS,
};

/// How many rows a placement's properties fill.
inline constexpr size_t EDITOR_PLACEMENT_FIELD_COUNT =
    sizeof(EDITOR_PLACEMENT_FIELDS) / sizeof(EDITOR_PLACEMENT_FIELDS[0]);

}  // namespace eng::editor
