#pragma once

/// @file logic-spawn.h
/// @brief An actor game logic asks the world to add.
/// @par Threading
/// A value type; its string views need live only until the call that
/// takes it returns.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/content/faction.h>
#include <string_view>

namespace eng::game {

/// An actor made to measure, for `GameLogicWorld::spawnActor`. For one of
/// the project's enemy archetypes, `spawnEnemy` is shorter.
struct LogicSpawn {
  /// Where its feet start, in tiles. Also its home.
  Vec3 at{};
  /// Which way it faces, in degrees counterclockwise from world +X.
  float yaw_degrees = 0.0F;
  /// The id of the behavior it runs: a built-in (`chase`, `guard`) or the
  /// project's own. One the project lacks runs `idle`.
  std::string_view behavior{};
  /// Which side it is on.
  Faction faction = Faction::HOSTILE;
  /// Health segments it starts with; at least one.
  uint16_t health = 3;
  /// What the logic calls it — what `LogicActor::id` reads back — or empty.
  std::string_view id{};
  /// What draws it: an asset reference (`mesh:grunt`), or empty for the
  /// stand-in. Never read by a tick.
  std::string_view model{};
};

}  // namespace eng::game
