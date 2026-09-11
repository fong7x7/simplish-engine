#pragma once

/// @file editor-actor-overlay.h
/// @brief What the viewport draws of one actor's mind during a playtest.
/// @par Threading Main-thread-only.

#include <editor/shell/iso-projection.h>
#include <engine/math/vec2.h>
#include <game/content/faction.h>
#include <string>
#include <vector>

namespace eng::editor {

/// One actor as the AI overlay shows it (Editor REQUIREMENTS §7, debug
/// overlays): where it stands and looks, how far it sees, the path it is
/// walking, what it is after, and the state it is in.
///
/// Copied out of the running game each frame the overlay is on, as the
/// playtest mirror is, so the viewport never reads the simulation.
/// @thread_safety Main-thread-only.
struct EditorActorOverlay {
  /// Where its feet are, as drawn this frame.
  WorldPoint at{};
  /// How tall it is, which puts its label above its head.
  float height = 0.0F;
  /// The unit direction it faces.
  Vec2 facing{};
  /// How wide its view is, in degrees; 360 or more sees all round.
  float view_degrees = 360.0F;
  /// How far it sees, in tiles.
  float sight_range = 0.0F;
  /// The waypoints of its path it has still to reach, in order.
  std::vector<WorldPoint> path;
  /// Whether it remembers a target — which `target` is then where.
  bool has_target = false;
  /// Where it last perceived its target.
  WorldPoint target{};
  /// Whether it sees its target this tick.
  bool sees_target = false;
  /// What its label says: the state it is in.
  std::string label;
  /// Which side it is on, which colours it.
  game::Faction faction = game::Faction::HOSTILE;
};

}  // namespace eng::editor
