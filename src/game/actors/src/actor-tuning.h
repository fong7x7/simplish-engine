#pragma once

/// @file actor-tuning.h
/// @brief The constants actor movement and planning are tuned by.
/// @par Threading
/// Constants.

#include <cstdint>

namespace eng::game {

/// How near its goal an actor walking to a place, rather than a target,
/// counts as arrived, in tiles.
inline constexpr float ACTOR_ARRIVE_TILES = 0.2F;

/// How near a waypoint an actor counts as having reached it and moves on
/// to the next, in tiles. More than the fastest step, so no actor steps
/// over a waypoint and turns back for it.
inline constexpr float ACTOR_WAYPOINT_TILES = 0.35F;

/// How many cells out a goal an actor cannot stand on — beside a crate,
/// inside one — looks for the nearest spot it can: two tiles.
inline constexpr uint32_t ACTOR_SNAP_RINGS = 8;

/// Ticks between replans of a path whose goal has moved: a quarter second.
/// A pursuer's goal moves with its quarry every few ticks; replanning each
/// time would be wasted, and a straight walk takes over anyway once the
/// quarry is in view.
inline constexpr uint64_t ACTOR_REPLAN_TICKS = 15;

/// The smallest displacement that counts as moving, in tiles.
inline constexpr float ACTOR_MIN_STEP_TILES = 0.002F;

/// The fraction of an intended step an actor must make to count as not
/// blocked. Sliding along a wall keeps more than this; walking into one
/// head-on does not.
inline constexpr float ACTOR_BLOCKED_FRACTION = 0.25F;

/// The chance per tick, in thousandths, that a wanderer standing at its
/// spot picks another: a pause of a second or so on average.
inline constexpr uint32_t ACTOR_WANDER_REPICK_PERMILLE = 15;

}  // namespace eng::game
