#pragma once

/// @file enemy-definition.h
/// @brief One enemy archetype: what the director spawns, as data.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/content/faction.h>
#include <string>

namespace eng::game {

/// How wide an archetype's body is when its row says nothing, in tiles:
/// an actor's default radius.
inline constexpr float ENEMY_DEFAULT_RADIUS_TILES = 0.3F;

/// How tall an archetype's body is when its row says nothing, in tiles:
/// an actor's default height.
inline constexpr float ENEMY_DEFAULT_HEIGHT_TILES = 1.5F;

/// How many health segments an archetype has when its row says nothing:
/// one, as a swarmer does.
inline constexpr uint16_t ENEMY_DEFAULT_HEALTH = 1;

/// An enemy archetype (Game REQUIREMENTS §5.1), as the enemies data table
/// defines one (project-format §8.3): the model a horde is drawn with, the
/// body it collides with, the behavior it runs, and its health.
///
/// What a hand-placed prop gets from its asset and its Behavior row, an
/// archetype carries itself, so the director can spawn one anywhere.
/// `makeEnemySpawn` turns one into the `ActorSpawn` a prop would have
/// become, so both routes into the game give it the same thing.
///
/// `health` is read and kept, and used by nothing yet: actors take damage
/// when weapons exist to deal it.
struct EnemyDefinition {
  /// Stable identifier, as the director and a replay name it: `swarmer`.
  std::string id{};
  /// What the editor calls it: `Swarmer`.
  std::string name{};
  /// The asset it is drawn as — `mesh:enemies_swarmer` — or empty for the
  /// stand-in. Presentation: the simulation never reads it.
  std::string model{};
  /// How many health segments it spawns with.
  uint16_t health = ENEMY_DEFAULT_HEALTH;
  /// Its body's radius, in tiles.
  float radius = ENEMY_DEFAULT_RADIUS_TILES;
  /// Its body's height, in tiles.
  float height = ENEMY_DEFAULT_HEIGHT_TILES;
  /// The id of the behavior it runs: a built-in or one of the project's.
  std::string behavior{};
  /// The side it is on.
  Faction faction = Faction::HOSTILE;
};

}  // namespace eng::game
