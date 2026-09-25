#pragma once

/// @file ring-spawn.h
/// @brief Actors spawned evenly round a circle: a wave coming in.
/// @par Threading
/// A value type; its views must outlive the call it is used in.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-spawn.h>
#include <game/sdk/ring-placement.h>
#include <string_view>

namespace eng::game::sdk {

/// `count` actors, evenly round a circle of `radius` tiles about `centre`,
/// the first at `start_degrees` counterclockwise from world +X, each
/// facing the centre. Each is the project's enemy archetype `enemy` when
/// that is named, else `actor` — whose `at` and `yaw_degrees` the ring
/// sets.
struct RingSpawn {
  /// The circle's centre.
  Vec3 centre{};
  /// Its radius, in tiles.
  float radius = 8.0F;
  /// How many to spawn.
  uint32_t count = 4;
  /// Where round the circle the first goes, in degrees.
  float start_degrees = 0.0F;
  /// An enemy archetype's id, or empty to spawn `actor`.
  std::string_view enemy{};
  /// What to spawn when no archetype is named; its `id` names every one.
  LogicSpawn actor{};
  /// Whether a spot inside a prop is skipped.
  RingPlacement placement = RingPlacement::WALKABLE_ONLY;
};

/// Spawn @p ring into @p world. How many were queued: fewer than asked for
/// when a spot was not walkable, the run ran out of room, or the archetype
/// is not the project's.
uint32_t spawnRing(GameLogicWorld& world, const RingSpawn& ring);

}  // namespace eng::game::sdk
