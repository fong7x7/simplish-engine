#pragma once

/// @file actor-path.h
/// @brief The route an actor is following: a few straight legs.
/// @par Threading
/// A value type.

#include <array>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/sim/state-hasher.h>
#include <engine/spatial/grid-cell.h>

namespace eng::game {

/// Most waypoints one path holds. A smoothed route across a level is a
/// handful of legs; a longer one is walked sixteen at a time, planning
/// again from the last.
inline constexpr uint32_t ACTOR_PATH_POINTS = 16;

/// A planned route: smoothed waypoints and how far along them the actor is.
/// Fixed-size, so a pool of them allocates nothing during a run.
struct ActorPath {
  /// The waypoints, in walking order; the first `count` are used.
  std::array<Vec2, ACTOR_PATH_POINTS> points{};
  /// How many waypoints the path has.
  uint32_t count = 0;
  /// The waypoint being walked to; `count` once they are all reached.
  uint32_t next = 0;
  /// The grid cell the path was planned to.
  spatial::GridCell goal{};
  /// The tick it was planned on, which limits how often it is replanned.
  uint64_t planned_tick = 0;
};

}  // namespace eng::game

namespace eng::sim {

/// `ActorPath` is floats and 32- and 64-bit integers laid out with no
/// padding, so its bytes are its value and a pool of them hashes as one
/// span.
template <>
inline constexpr bool IS_HASHABLE_BITS<game::ActorPath> =
    sizeof(game::ActorPath) == sizeof(Vec2) * game::ACTOR_PATH_POINTS +
                                   2 * sizeof(uint32_t) +
                                   sizeof(spatial::GridCell) + sizeof(uint64_t);

}  // namespace eng::sim
