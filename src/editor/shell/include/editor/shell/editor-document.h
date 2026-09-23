#pragma once

/// @file editor-document.h
/// @brief Everything the editor has authored into the level so far.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-emitter.h>
#include <editor/shell/editor-light.h>
#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-player-start.h>
#include <editor/shell/editor-sprite.h>
#include <editor/shell/editor-waypoint.h>
#include <engine/render-ground/ground-grid.h>
#include <vector>

namespace eng::editor {

/// The level as the editor holds it: what has been placed, what lights it,
/// where the players enter it, the routes its actors patrol, the particle
/// emitters that show effects in it, the sprite billboards standing in
/// it, and the terrain its floor is painted with.
///
/// One record rather than a list per kind passed around separately, because
/// the history describes all of it: an action names a list and a slot in
/// it, and undo has to reach whichever list that was. This is what a level
/// file holds: `editor-level-json.h` writes it and reads it back.
/// @thread_safety Main-thread-only.
struct EditorDocument {
  /// Assets placed in the world, in the order they were placed.
  std::vector<EditorPlacement> placements;
  /// Lights placed in the world, in the order they were added.
  std::vector<EditorLight> lights;
  /// Where players spawn, in the order the starts were added.
  std::vector<EditorPlayerStart> player_starts;
  /// The points of the level's patrol routes, in the order they were added.
  std::vector<EditorWaypoint> waypoints;
  /// Particle emitters, in the order they were added.
  std::vector<EditorEmitter> emitters;
  /// Sprite billboards, in the order they were added.
  std::vector<EditorSprite> sprites;
  /// The terrain painted on each cell of the floor, numbered as
  /// `EDITOR_TERRAINS` is: 0 is bare, 1 its first terrain.
  GroundGrid ground;
};

}  // namespace eng::editor
