#pragma once

/// @file actor-spawn.h
/// @brief Where an actor enters a run, and as what.
/// @par Threading
/// A value type.

#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <game/content/faction.h>
#include <string>
#include <vector>

namespace eng::game {

/// How wide an actor is to collision when nothing says otherwise: a
/// player's width (`PLAYER_RADIUS_TILES`).
inline constexpr float ACTOR_DEFAULT_RADIUS_TILES = 0.3F;

/// How tall an actor is to collision when nothing says otherwise: a
/// player's height.
inline constexpr float ACTOR_DEFAULT_HEIGHT_TILES = 1.5F;

/// One actor a run starts with — part of `GameSetup`, so part of what the
/// simulation is a function of. Plain values, like a player's spawn: the
/// game never sees the editor's prop it came from.
struct ActorSpawn {
  /// Where its feet start, in tiles. Also its home.
  Vec3 at{};
  /// Which way it starts facing, in degrees counterclockwise from world +X.
  /// Degrees rather than a direction, so the simulation turns it into one
  /// with its own deterministic trigonometry and every peer agrees on it.
  float yaw_degrees = 0.0F;
  /// The id of the behavior it runs; one the content lacks runs `idle`.
  std::string behavior{};
  /// Which side it is on.
  Faction faction = Faction::HOSTILE;
  /// Its radius to collision, in tiles.
  float radius = ACTOR_DEFAULT_RADIUS_TILES;
  /// Its height to collision, in tiles.
  float height = ACTOR_DEFAULT_HEIGHT_TILES;
  /// The waypoints it patrols, in walking order; empty for none. What a
  /// `patrol` state walks.
  std::vector<Vec2> route{};
};

}  // namespace eng::game
