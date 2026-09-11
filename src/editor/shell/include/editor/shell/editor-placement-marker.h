#pragma once

/// @file editor-placement-marker.h
/// @brief What the viewport draws and picks for one placement.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/editor-placement-bounds.h>
#include <engine/math/vec2.h>
#include <game/content/faction.h>

namespace eng::editor {

/// How the viewport draws a marker.
///
/// A drawing style rather than which list the marker came from: the
/// viewport still knows nothing about assets, lights or players, only that
/// some markers stand up off the ground and want to be seen doing it.
/// @thread_safety Immutable value type.
enum class EditorMarkerStyle : uint8_t {
  /// The outline of the box's footprint on the ground: a prop, a light.
  FOOTPRINT,
  /// The same footprint, dimmed: a prop players walk through, so a level
  /// shows at a glance which of its props will stop somebody.
  PASSABLE,
  /// The whole box, always, in the colour of the player it starts — a
  /// player start, which has nothing else in the scene to see it by.
  PLAYER_START,
  /// The footprint in its faction's colour, with a tick from its middle the
  /// way it faces — a prop with a behavior, so a level shows at a glance
  /// which props will move, whose side they are on, and where they look.
  ACTOR,
  /// The whole box, in its route's colour — a patrol route's waypoint,
  /// a post with nothing else in the scene to see it by.
  WAYPOINT,
  /// The whole box, in a hot orange — a projectile in flight in a
  /// playtest, which has no model to see it by.
  PROJECTILE,
  /// The footprint, in an acid green — a hazard pool on the floor in a
  /// playtest.
  HAZARD,
};

/// One thing in the level, as much of it as the viewport needs.
///
/// The viewport knows nothing about assets, meshes, lights, or which list
/// an entry came from — it is handed a box and told whether it is the
/// selected one. That keeps the widget testable without a project, keeps
/// the editor the single place that decides what is selected, and is what
/// let lights reuse the overlay and the picking a placement already had.
/// @thread_safety Main-thread-only.
struct EditorPlacementMarker {
  /// World box the placement occupies.
  PlacementBounds bounds{};
  /// Whether this is the selected placement.
  bool selected = false;
  /// How it is drawn.
  EditorMarkerStyle style = EditorMarkerStyle::FOOTPRINT;
  /// The player a `PLAYER_START` marker starts, 1 to 4, which picks its
  /// colour. Unused by every other style.
  uint8_t player = 0;
  /// The side an `ACTOR` marker is on, which picks its colour.
  game::Faction faction = game::Faction::HOSTILE;
  /// The unit direction an `ACTOR` marker faces, on the floor.
  Vec2 facing{};
  /// The route a `WAYPOINT` marker belongs to, 1 to 9, which picks its
  /// colour.
  uint8_t route = 0;
};

}  // namespace eng::editor
