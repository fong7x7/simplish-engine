#pragma once

/// @file editor-playtest-actor.h
/// @brief One actor in a running playtest, as the editor reports it.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec2.h>
#include <game/content/faction.h>
#include <string>

namespace eng::editor {

/// What one actor was doing on the tick a playtest last ran — what the
/// agent API reports, copied out of the simulation rather than read from
/// it.
/// @thread_safety Main-thread-only.
struct EditorPlaytestActor {
  /// The id of the prop it is: `characters_knight_01`.
  std::string id{};
  /// Where its feet are, in tiles.
  WorldPoint position{};
  /// The unit direction it faces.
  Vec2 facing{};
  /// The id of the behavior it runs.
  std::string behavior{};
  /// The id of the state its behavior is in: `pursue`.
  std::string state{};
  /// Which side it is on.
  game::Faction faction = game::Faction::HOSTILE;
  /// The player it has as its target, 1 to 4, or 0 for none — and 0 when
  /// its target is another actor.
  uint8_t target = 0;
  /// The id of the actor it has as its target, empty when its target is a
  /// player or it has none.
  std::string target_actor{};
  /// Whether it sees its target this tick.
  bool sees_target = false;
  /// Waypoints left on the path it is following; 0 when it walks straight.
  uint32_t path_waypoints = 0;
  /// Health segments it has left.
  uint16_t health = 0;
  /// Health segments it spawned with.
  uint16_t max_health = 0;
};

}  // namespace eng::editor
