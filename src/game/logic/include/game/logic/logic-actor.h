#pragma once

/// @file logic-actor.h
/// @brief One actor, as game logic reads it.
/// @par Threading
/// A value type; its string views live as long as the tick it was read in.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <game/content/faction.h>
#include <game/logic/logic-target.h>
#include <string_view>

namespace eng::game {

/// A copy of one actor's state at the moment it was read. Changing it
/// changes nothing: game logic writes through `GameLogicWorld`.
///
/// The two names are views into the world's own strings. They are valid
/// until the logic's `tick` returns; copy one to keep it longer.
struct LogicActor {
  /// Who this is, for `GameLogicWorld::damage` and `heal`.
  LogicTarget target{};
  /// The id the level gave it — the prop it was placed as in the editor —
  /// or empty when whoever set the run up gave none.
  std::string_view id{};
  /// Where its feet are, in tiles.
  Vec3 position{};
  /// The unit direction it faces, world X and Y.
  Vec2 facing{};
  /// Which side it is on.
  Faction faction = Faction::HOSTILE;
  /// Health segments left; none means it dies at the end of this tick.
  uint16_t health = 0;
  /// Health segments it spawned with.
  uint16_t max_health = 0;
  /// The id of the state of its behavior it is in: `chase`, `search`.
  std::string_view state{};
};

}  // namespace eng::game
